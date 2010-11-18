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

#ifndef BINARYHEAP_H_INCLUDED
#define BINARYHEAP_H_INCLUDED

//Not compatible with non contiguous node ids

#include <cassert>
#include <vector>
#include <QHash>

template< typename NodeID, typename Key >
class ArrayStorage {
	public:

		ArrayStorage( size_t size ) :
				positions( new Key[size] ), size( size )
		{
		}

		~ArrayStorage()
		{
			delete[] positions;
		}

		Key &operator[]( NodeID node )
		{
			assert( (std::size_t)node < size );
			return positions[node];
		}

		void clear() {}

	private:
		Key* positions;
		std::size_t size;
};

template< typename NodeID, typename Key >
class MapStorage {
	public:

		MapStorage( size_t )
		{
		}

		Key &operator[]( NodeID node )
		{
			return nodes[node];
		}

		void clear()
		{
			nodes.clear();
		}

	private:
		QHash< NodeID, Key > nodes;

};

template < typename NodeID, typename Key, typename Weight, typename Data, typename IndexStorage = ArrayStorage< NodeID, Key > >
class BinaryHeap {
	private:
		BinaryHeap( const BinaryHeap& right );
		void operator=( const BinaryHeap& right );
	public:
		typedef Weight WeightType;
		typedef Data DataType;

		BinaryHeap( size_t maxID )
				: nodeIndex( maxID ) {
			Clear();
		}

		void Clear() {
			heap.resize( 1 );
			insertedNodes.clear();
			nodeIndex.clear();
			heap[0].weight = 0;
		}

		Key Size() const {
			return ( Key )( heap.size() - 1 );
		}

		void Insert( NodeID node, Weight weight, const Data &data ) {
			HeapElement element;
			element.index = ( NodeID ) insertedNodes.size();
			element.weight = weight;
			const Key key = ( Key ) heap.size();
			heap.push_back( element );
			insertedNodes.push_back( HeapNode( node, key, weight, data ) );
			nodeIndex[node] = element.index;
			Upheap( key );
			CheckHeap();
		}

		Data& GetData( NodeID node ) {
			const Key index = nodeIndex[node];
			return insertedNodes[index].data;
		}

		Weight& GetKey( NodeID node ) {
			const Key index = nodeIndex[node];
			return insertedNodes[index].weight;
		}

		bool WasRemoved( NodeID node ) {
			assert( WasInserted( node ) );
			const Key index = nodeIndex[node];
			return insertedNodes[index].key == 0;
		}

		bool WasInserted( NodeID node ) {
			const Key index = nodeIndex[node];
			if ( index >= ( Key ) insertedNodes.size() )
				return false;
			return insertedNodes[index].node == node;
		}

		NodeID Min() const {
			assert( heap.size() > 1 );
			return insertedNodes[heap[1].index].node;
		}

		const Weight& MinKey() const {
			assert( heap.size() > 1 );
			return insertedNodes[heap[1].index].weight;
		}

		NodeID DeleteMin() {
			assert( heap.size() > 1 );
			const Key removedIndex = heap[1].index;
			heap[1] = heap[heap.size()-1];
			heap.pop_back();
			if ( heap.size() > 1 )
				Downheap( 1 );
			insertedNodes[removedIndex].key = 0;
			CheckHeap();
			return insertedNodes[removedIndex].node;
		}

		void DeleteAll() {
			for ( typename std::vector< HeapElement >::iterator i = heap.begin() + 1, iend = heap.end(); i != iend; ++i )
				insertedNodes[i->index].key = 0;
			heap.resize( 1 );
			heap[0].weight = 0;
		}


		void DecreaseKey( NodeID node, Weight weight ) {
			const Key index = nodeIndex[node];
			Key key = insertedNodes[index].key;
			assert ( key != 0 );

			insertedNodes[index].weight = weight;
			heap[key].weight = weight;
			Upheap( key );
			CheckHeap();
		}

	private:
		class HeapNode {
			public:
				HeapNode() {
				}
				HeapNode( NodeID n, Key k, Weight w, Data d )
						: node( n ), key( k ), weight( w ), data( d ) {
				}

				NodeID node;
				Key key;
				Weight weight;
				Data data;
		};
		struct HeapElement {
			Key index;
			Weight weight;
		};

		std::vector< HeapNode > insertedNodes;
		std::vector< HeapElement > heap;
		IndexStorage nodeIndex;

		void Downheap( Key key ) {
			assert( (std::size_t)key < heap.size() );
			const Key droppingIndex = heap[key].index;
			const Weight weight = heap[key].weight;
			Key nextKey = key << 1;
			while ( nextKey < ( Key ) heap.size() ) {
				const Key nextKeyOther = nextKey + 1;
				if ( ( nextKeyOther < ( Key ) heap.size() ) )
					if ( heap[nextKey].weight > heap[nextKeyOther].weight )
						nextKey = nextKeyOther;

				if ( weight <= heap[nextKey].weight )
					break;

				heap[key] = heap[nextKey];
				assert( (std::size_t)heap[key].index < insertedNodes.size() );
				insertedNodes[heap[key].index].key = key;
				key = nextKey;
				nextKey <<= 1;
			}
			heap[key].index = droppingIndex;
			heap[key].weight = weight;
			assert( (std::size_t)droppingIndex < insertedNodes.size() );
			insertedNodes[droppingIndex].key = key;
		}

		void Upheap( Key key ) {
			assert( (std::size_t)key < heap.size() );
			const Key risingIndex = heap[key].index;
			const Weight weight = heap[key].weight;
			Key nextKey = key >> 1;
			assert( (std::size_t)nextKey < heap.size() );
			while ( heap[nextKey].weight > weight ) {
				assert( nextKey != 0 );
				assert( (std::size_t)nextKey < heap.size() );
				heap[key] = heap[nextKey];
				assert( (std::size_t)heap[key].index < insertedNodes.size() );
				insertedNodes[heap[key].index].key = key;
				key = nextKey;
				nextKey >>= 1;
			}
			heap[key].index = risingIndex;
			heap[key].weight = weight;
			assert( (std::size_t)risingIndex < insertedNodes.size() );
			insertedNodes[risingIndex].key = key;
		}
		
		void CheckHeap() {
			/*for ( Key i = 2; i < heap.size(); ++i ) {
				assert( heap[i].weight >= heap[i >> 1].weight );
			}*/
		}
};

#endif //#ifndef BINARYHEAP_H_INCLUDED
