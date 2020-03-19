#include "../Include/kdtree.h"
#include <fstream>

using namespace std;

Node::Node()
	: m_axis(EDimension::NO_DIM)
{}

Node::Node(const Vector& p, const Photon& photon)
	: m_axis(EDimension::NO_DIM)
	, m_point(p)
	, m_photon(photon)
{}

Vector Node::GetPoint() const
{
	return m_point;
}

Photon Node::GetPhoton() const
{
	return m_photon;
}


void KDTree::Clear()
{
	m_nodes.clear();
	m_balanced.clear();
}

void KDTree::Store(const Vector& point, const Photon& photon)
{
	m_nodes.push_back(Node(point, photon));
}

int KDTree::Find(const Vector& p, float radius, list<Node*>* nodes) const
{
	if (nodes)
	{
		Find(p, 1, radius, *nodes);
		return nodes->size();
	}
	else
	{
		list<Node*> local_nodes;
		Find(p, 1, radius, local_nodes);

		return local_nodes.size();
	}
}

int KDTree::Size() const
{
	return m_balanced.size();
}

bool KDTree::IsEmpty() const
{
	return m_balanced.empty();
}

const Node& KDTree::operator[](const unsigned int index) const
{
	return m_balanced[index];
}

void KDTree::Find(const Vector& point, int nb_elements, vector<Node*>& nodes, float &max_distance) const
{
	nodes.clear();
	max_distance = numeric_limits<float>::infinity();

	if (m_balanced.empty())
		return;

	nodes.reserve(nb_elements);
	vector<pair<int, float> > dist;
	dist.reserve(nb_elements);

	Find(point, 1, nb_elements, max_distance, nodes, dist);
}

const Node& KDTree::Find(const Vector& p) const
{
	return m_balanced[Closest(p, 1, 1)];
}

void KDTree::Find(const Vector& p, int index, float radius, list<const Node*>& nodes) const
{
	if (m_balanced[index].m_point.Distance(p) < radius)
	{
		nodes.push_back(&m_balanced[index]);
	}

	if (index < ((m_balanced.size() - 1) / 2))
	{
		float distaxis = p[m_balanced[index].m_axis] - m_balanced[index].m_point[m_balanced[index].m_axis];
		if (distaxis < 0.0f)
		{
			Find(p, index * 2, radius, nodes);
			if (radius > fabs(distaxis))
				Find(p, 2 * index + 1, radius, nodes);
		}
		else
		{
			Find(p, 2 * index + 1, radius, nodes);
			if (radius > fabs(distaxis))
			{
				Find(p, 2 * index, radius, nodes);
			}
		}
	}
}

void KDTree::UpdateHeapNodes(Node& node, float distance, int nb_elements, vector<Node*>& nodes, vector<pair<int, float> >&dist) const
{
	if (nodes.size() < nb_elements)
	{
		dist.push_back(pair<int, float>(nodes.size(), distance));
		nodes.push_back(&node);

		if (nodes.size() == nb_elements)
		{
			make_heap(dist.begin(), dist.end(), HeapComparison());
		}
	}
	else
	{
		int idx = dist.front().first;
		nodes[idx] = &node;
		pop_heap(dist.begin(), dist.end(), HeapComparison());
		dist.pop_back();
		dist.push_back(pair<int, float>(idx, distance));
		push_heap(dist.begin(), dist.end(), HeapComparison());
	}
}

void KDTree::Find(const Vector& p, int index, int nb_elements, float &dist_worst, vector<Node*>& nodes, vector<pair<int, float> >&dist) const
{

}

int KDTree::Closest(const Vector& p, int index, int best) const
{

}

void KDTree::MedianSplit(vector<Node>& p, int start, int end, int median, EDimension& axis)
{

}

void KDTree::BalanceSegment(vector<Node>& pbal, vector<Node>& porg, int index, int start, int end, const Vector& bbmin, const Vector& bbmax)
{

}

void KDTree::Balance()
{

}

void KDTree::DumpToFile(const string& filename)
{

}
