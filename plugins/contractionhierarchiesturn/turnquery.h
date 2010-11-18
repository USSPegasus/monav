/*
Copyright 2010  Christian Vetter veaac.fdirct@gmail.com

This file is part of MoNav.

MoNav is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoNav is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoNav.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TURNQUERY_H_INCLUDED
#define TURNQUERY_H_INCLUDED
#include <vector>
#include <omp.h>
#include <limits.h>
#include <limits>
#include <sstream>
#include "utils/qthelpers.h"
#include "dynamicturngraph.h"
#include "../contractionhierarchies/binaryheap.h"
#include "utils/config.h"

template <typename Graph, bool StallOnDemand = true >
class TurnQuery {
public:
	typedef typename Graph::NodeIterator NodeIterator;
	typedef typename Graph::EdgeIterator EdgeIterator;
	typedef typename Graph::EdgeData EdgeData;
	typedef typename Graph::PenaltyData PenaltyData;

	TurnQuery(const Graph& graph) : m_graph( graph ), m_heapForward( graph.GetNumberOfOriginalEdges() ),
			m_heapBackward( graph.GetNumberOfOriginalEdges() ) {}

public:
	const Graph& m_graph;
    static const PenaltyData RESTRICTED_TURN = 255;

	struct HeapData {
		NodeIterator parentOrig;
		EdgeIterator parentEdge;
		NodeIterator node;
		unsigned char originalEdge : 8;
		bool stalled: 1;
		HeapData( NodeIterator po, EdgeIterator pe, NodeIterator n, unsigned o ) : parentOrig(po), parentEdge(pe), node(n), originalEdge(o), stalled(false) {}
		std::string DebugString() const {
			std::stringstream ss;
			ss << parentOrig << " -- " << parentEdge << " -- " << (unsigned)originalEdge << " -> " << node;
			if (stalled)
				ss << " STALLED";
			return ss.str();
		}
	};
	class AllowForwardEdge {
		public:
			bool operator()( bool forward, bool /*backward*/ ) const {
				return forward;
			}
	};

	class AllowBackwardEdge {
		public:
			bool operator()( bool /*forward*/, bool backward ) const {
				return backward;
			}
	};

	class ComputePenaltyForward {
		public:
			PenaltyData operator()( const Graph& g, const NodeIterator& n, unsigned short o1, unsigned short o2 ) const {
				return g.GetPenaltyData( n, o1, o2 );
			}
	};
	class ComputePenaltyBackward {
		public:
			PenaltyData operator()( const Graph& g, const NodeIterator& n, unsigned short o1, unsigned short o2 ) const {
				return g.GetPenaltyData( n, o2, o1 );
			}
	};


	typedef BinaryHeap< NodeIterator, unsigned, int, HeapData > Heap;

	Heap m_heapForward;
	Heap m_heapBackward;

	struct StallQueueItem
	{
		NodeIterator node;
		unsigned originalEdge;
		int distance;
		StallQueueItem(NodeIterator n, unsigned o, int d) : node(n), originalEdge(o), distance(d) {}
	};
	std::queue< StallQueueItem > m_stallQueue;

	struct Middle {
		NodeIterator node;
		unsigned in;
		unsigned out;
		Middle(): node(-1), in(-1), out(-1) {}
		Middle(NodeIterator n, unsigned i, unsigned o) : node(n), in(i), out(o) {}
	} m_middle;

	class SetMiddleForward {
		public:
		Middle operator()( const NodeIterator& n, unsigned i, unsigned o ) const {
				return Middle(n, i, o);
		}
	};
	class SetMiddleBackward {
		public:
		Middle operator()( const NodeIterator& n, unsigned i, unsigned o ) const {
			return Middle(n, o, i);
		}
	};



	template <class EdgeAllowed>
	void initHeap( Heap* heap, const NodeIterator node, const NodeIterator node2, const EdgeAllowed& edgeAllowed ) {
//		qDebug() << m_graph.DebugStringEdgesOf( node ).c_str();
		for ( EdgeIterator edge = m_graph.BeginEdges( node ), edgeEnd = m_graph.EndEdges( node ); edge < edgeEnd; ++edge ) {
			const EdgeData& edgeData = m_graph.GetEdgeData( edge );
			const NodeIterator edgeTarget = m_graph.GetTarget( edge );
			if ( !edgeAllowed(edgeData.forward, edgeData.backward) || edgeData.shortcut || edgeTarget != node2 )
				continue;

			const unsigned originalEdgeLocal = m_graph.GetOriginalEdgeTarget( edge );
			const unsigned originalEdge = m_graph.GetFirstOriginalEdge( node2 ) + originalEdgeLocal;
			HeapData data(-1, edge, node2, originalEdgeLocal);
//			qDebug() << "edge orig =" << edgeData.id << "originalEdge" << originalEdge;
			if ( !heap->WasInserted( originalEdge ) ) {
//				qDebug() << "  insert";
				heap->Insert( originalEdge, edgeData.distance, data );
			}
			else if ( (int)edgeData.distance < heap->GetKey( originalEdge ) ) {
//				qDebug() << "  decrease";
				heap->DecreaseKey( originalEdge, edgeData.distance );
				heap->GetData( originalEdge ) = data;
			}
		}
//		qDebug() << m_graph.DebugStringEdgesOf( node2 ).c_str();
		for ( EdgeIterator edge = m_graph.BeginEdges( node2 ), edgeEnd = m_graph.EndEdges( node2 ); edge < edgeEnd; ++edge ) {
			const EdgeData& edgeData = m_graph.GetEdgeData( edge );
			const NodeIterator edgeTarget = m_graph.GetTarget( edge );
			if ( !edgeAllowed(edgeData.backward, edgeData.forward) || edgeData.shortcut || edgeTarget != node )
				continue;

			const unsigned originalEdgeLocal = m_graph.GetOriginalEdgeSource( edge );
			const unsigned originalEdge = m_graph.GetFirstOriginalEdge( node2 ) + originalEdgeLocal;
			HeapData data(-1, edge, node2, originalEdgeLocal);
//			qDebug() << "edge orig =" << edgeData.id << "originalEdge" << originalEdge;
			if ( !heap->WasInserted( originalEdge ) ) {
//				qDebug() << "  insert";
				heap->Insert( originalEdge, edgeData.distance, data );
			}
			else if ( (int)edgeData.distance < heap->GetKey( originalEdge ) ) {
//				qDebug() << "  decrease";
				heap->DecreaseKey( originalEdge, edgeData.distance );
				heap->GetData( originalEdge ) = data;
			}
		}
	}

	template< class EdgeAllowed, class StallEdgeAllowed, class PenaltyFunction, class MiddleFunction >
	unsigned computeStep( Heap* heapForward, Heap* heapBackward, const EdgeAllowed& edgeAllowed,
		const StallEdgeAllowed& stallEdgeAllowed,
		unsigned (Graph::*degIn)( const NodeIterator &n )const, unsigned (Graph::*degOut)( const NodeIterator &n )const,
		const PenaltyFunction& penaltyFunction, const MiddleFunction& middleFunction,  int* targetDistance ) {

		const unsigned originalEdge = heapForward->DeleteMin();
		const int distance = heapForward->GetKey( originalEdge );
		HeapData data = heapForward->GetData( originalEdge );  // do not use a reference to the data, as it will become invalid on a insert
//		qDebug() << "settle" << originalEdge << "distance" << distance << "data" << data.DebugString().c_str();

		if ( StallOnDemand && data.stalled )
			return originalEdge;

		{
//			qDebug() << "check meeting";
			unsigned deg = (m_graph.*degOut)( data.node );
			unsigned firstOriginal = m_graph.GetFirstOriginalEdge( data.node );
			for ( unsigned out = 0; out < deg; ++out ) {
				const unsigned orig = firstOriginal + out;
				if ( heapBackward->WasInserted( orig ) && !heapBackward->GetData( orig ).stalled ) {
					PenaltyData penalty = penaltyFunction(m_graph, data.node, data.originalEdge, out);
					if (penalty == RESTRICTED_TURN)
						continue;
					const int newDistance = heapBackward->GetKey( orig ) + penalty + distance;
					if ( newDistance < *targetDistance ) {
//						qDebug() << "=== new ===" << newDistance << "===" << (unsigned)data.originalEdge << "--" << data.node << "--" << out;
						m_middle = middleFunction(data.node, originalEdge, orig);
						*targetDistance = newDistance;
					}
				}
			}
		}

		if ( distance > *targetDistance ) {
			heapForward->DeleteAll();
			return originalEdge;
		}

//		qDebug() << "orig in" << m_graph.GetOriginalInDegree( data.node ) << "out" << m_graph.GetOriginalOutDegree( data.node );
//		qDebug() << m_graph.DebugStringEdgesOf( data.node ).c_str();
//		qDebug() << m_graph.DebugStringPenaltyData( data.node ).c_str();

		for ( EdgeIterator edge = m_graph.BeginEdges( data.node ), edgeEnd = m_graph.EndEdges( data.node ); edge < edgeEnd; ++edge ) {
			const EdgeData& edgeData = m_graph.GetEdgeData( edge );
			const NodeIterator to = m_graph.GetTarget( edge );
			assert( edgeData.distance > 0 );
			unsigned firstOriginalTo = m_graph.GetFirstOriginalEdge( to );
			unsigned originalEdgeLocalTo = m_graph.GetOriginalEdgeTarget( edge );

			if ( StallOnDemand && stallEdgeAllowed( edgeData.forward, edgeData.backward ) ) {
				int shorterDistance = std::numeric_limits<int>::max();
				unsigned deg = (m_graph.*degIn)( to );
				for ( unsigned in = 0; in < deg; ++in ) {
					const unsigned orig = firstOriginalTo + in;
					if ( heapForward->WasInserted( orig ) ) {
						PenaltyData penalty = penaltyFunction( m_graph, to, in, originalEdgeLocalTo );
						if ( penalty == RESTRICTED_TURN ) continue;
						shorterDistance = std::min( shorterDistance, heapForward->GetKey( orig ) + (int)penalty + (int)edgeData.distance );
					}
				}
				if ( shorterDistance < distance ) {
					//perform a bfs starting at node
					//only insert nodes when a sub-optimal path can be proven
					//insert node into the stall queue
					heapForward->GetKey( originalEdge ) = shorterDistance;
					heapForward->GetData( originalEdge ).stalled = true;
					m_stallQueue.push( StallQueueItem( data.node, data.originalEdge, shorterDistance ) );

					while ( !m_stallQueue.empty() ) {
						//get node from the queue
						const StallQueueItem& stallItem = m_stallQueue.front();
						m_stallQueue.pop();
						const int stallDistance = stallItem.distance;

						//iterate over outgoing edges
						for ( EdgeIterator stallEdge = m_graph.BeginEdges( stallItem.node ), stallEdgeEnd = m_graph.EndEdges( stallItem.node ); stallEdge < stallEdgeEnd; ++stallEdge ) {
							const EdgeData& stallEdgeData = m_graph.GetEdgeData( stallEdge );
							//is edge outgoing/reached/stalled?
							if ( !edgeAllowed( stallEdgeData.forward, stallEdgeData.backward ) )
								continue;
							const NodeIterator stallTo = m_graph.GetTarget( stallItem.node );
							const unsigned stallOrig = m_graph.GetFirstOriginalEdge( stallTo ) + m_graph.GetOriginalEdgeTarget( stallEdge );
							if ( !heapForward->WasInserted( stallOrig ) )
								continue;
							if ( heapForward->GetData( stallOrig ).stalled == true )
								continue;
							PenaltyData penalty = penaltyFunction( m_graph, stallItem.node, stallItem.originalEdge, m_graph.GetOriginalEdgeSource( stallEdge ) );
							if ( penalty == RESTRICTED_TURN )
								continue;


							const int stallToDistance = stallDistance + penalty + stallEdgeData.distance;
							//sub-optimal path found -> insert stallTo
							if ( stallToDistance < heapForward->GetKey( stallOrig ) ) {
								if ( heapForward->WasRemoved( stallOrig ) )
									heapForward->GetKey( stallOrig ) = stallToDistance;
								else
									heapForward->DecreaseKey( stallOrig, stallToDistance );

								m_stallQueue.push( StallQueueItem( stallTo, m_graph.GetOriginalEdgeTarget( stallEdge ), stallToDistance ) );
								heapForward->GetData( stallOrig ).stalled = true;
							}
						}
					}
					break;
				}
			}

			if ( edgeAllowed( edgeData.forward, edgeData.backward ) ) {
//				qDebug() << "edge allowed" << m_graph.DebugStringEdge( edge ).c_str() << "||" << data.node << data.originalEdge << m_graph.GetOriginalEdgeSource( edge );
				PenaltyData penalty = penaltyFunction( m_graph, data.node, data.originalEdge, m_graph.GetOriginalEdgeSource( edge ) );
//				qDebug() << "penalty" << penalty;
				if ( penalty == RESTRICTED_TURN )
					continue;

				const unsigned orig = firstOriginalTo + originalEdgeLocalTo;
				HeapData toData( originalEdge, edge, to, originalEdgeLocalTo );
				const int toDistance = distance + penalty + edgeData.distance;
//				qDebug() << orig << toDistance << toData.DebugString().c_str();
//				if (heapForward->WasInserted(orig)) qDebug() << heapForward->GetKey( orig );

				//New Node discovered -> Add to Heap + Node Info Storage
				if ( !heapForward->WasInserted( orig ) ) {
//					qDebug() << "  insert";
					heapForward->Insert( orig, toDistance, toData );

				//Found a shorter Path -> Update distance
				} else if ( toDistance <= heapForward->GetKey( orig ) ) {
//					qDebug() << "  update";
					heapForward->DecreaseKey( orig, toDistance );
					//new parent + unstall
					heapForward->GetData( orig ) = toData;
				}
			}
		}
		return originalEdge;
	}

public:
	int BidirSearch( const NodeIterator source, const NodeIterator source2, const NodeIterator target, const NodeIterator target2 ) {
		assert( source < m_graph.GetNumberOfNodes() );
		assert( source2 < m_graph.GetNumberOfNodes() );
		assert( target < m_graph.GetNumberOfNodes() );
		assert( target2 < m_graph.GetNumberOfNodes() );

		qDebug() << source << "->" << source2 << "---" << target2 << "->" << target;


		AllowForwardEdge forward;
		AllowBackwardEdge backward;
		initHeap( &m_heapForward, source, source2, forward );
		initHeap( &m_heapBackward, target, target2, backward );

		int targetDistance = std::numeric_limits< int >::max();
		if (m_heapBackward.Size() == 0 || m_heapForward.Size() == 0)
			return targetDistance;
		if (source == target2 && source2 == target) {
			assert( m_heapBackward.MinKey() == m_heapForward.MinKey() );
			return m_heapForward.MinKey();
		}



		ComputePenaltyForward penaltyForward;
		ComputePenaltyBackward penaltyBackward;
		SetMiddleForward middleForward;
		SetMiddleBackward middleBackward;
		while ( m_heapForward.Size() + m_heapBackward.Size() > 0 ) {

			if ( m_heapForward.Size() > 0 )
				computeStep( &m_heapForward, &m_heapBackward, forward, backward,
					&Graph::GetOriginalInDegree, &Graph::GetOriginalOutDegree,
					penaltyForward, middleForward, &targetDistance );

			if ( m_heapBackward.Size() > 0 )
				computeStep( &m_heapBackward, &m_heapForward, backward, forward,
					&Graph::GetOriginalOutDegree, &Graph::GetOriginalInDegree,
					penaltyBackward, middleBackward, &targetDistance );

		}

		return targetDistance;
	}

	int UnidirSearch( const NodeIterator source, const NodeIterator source2, const NodeIterator target, const NodeIterator target2 ) {
		assert( source < m_graph.GetNumberOfNodes() );
		assert( source2 < m_graph.GetNumberOfNodes() );
		assert( target < m_graph.GetNumberOfNodes() );
		assert( target2 < m_graph.GetNumberOfNodes() );


//		qDebug() << source << "->" << source2 << "---" << target2 << "->" << target;

		AllowForwardEdge forward;
		AllowBackwardEdge backward;
		initHeap( &m_heapForward, source, source2, forward );
		initHeap( &m_heapBackward, target, target2, backward );

		int targetDistance = std::numeric_limits< int >::max();
		if (m_heapBackward.Size() == 0 || m_heapForward.Size() == 0)
			return targetDistance;
		if (source == target2 && source2 == target) {
			assert( m_heapForward.MinKey() == m_heapBackward.MinKey() );
			return m_heapForward.MinKey();
		}

		ComputePenaltyForward penaltyForward;
		SetMiddleForward middleForward;
		unsigned numSettled = 0;
		while ( m_heapForward.Size() > 0 ) {
			++numSettled;
			computeStep( &m_heapForward, &m_heapBackward, forward, backward,
				&Graph::GetOriginalInDegree, &Graph::GetOriginalOutDegree, penaltyForward,
				middleForward, &targetDistance );
		}
//		qDebug() << "dist" << targetDistance;
//		exit(1);

		return targetDistance;
	}

	struct Path
	{
		std::vector<EdgeIterator> up;
		std::vector<EdgeIterator> down;
	};

	void GetPath(Path* path) {
		assert( m_middle.node != (NodeIterator)-1 );

		{
			unsigned origUp = m_middle.in;
			do {
//				qDebug() << "up" << origUp;
				assert( m_heapForward.WasInserted( origUp ) );
				const HeapData& data = m_heapForward.GetData( origUp );
				path->up.push_back( data.parentEdge );
				origUp = data.parentOrig;
			} while ( origUp != (unsigned)-1 );
		}

		{
			unsigned origDown = m_middle.out;
			do {
//				qDebug() << "down" << origDown;
				assert( m_heapBackward.WasInserted( origDown ) );
				const HeapData& data = m_heapBackward.GetData( origDown );
				path->down.push_back( data.parentEdge );
				origDown = data.parentOrig;
			} while ( origDown != (unsigned)-1 );
		}
	}

	void Clear() {
		m_middle = Middle(-1, 0, 0);
		m_heapForward.Clear();
		m_heapBackward.Clear();
	}





};

#endif // TURNQUERY_H_INCLUDED