#include "../Include/kdtree.h"
#include <fstream>

using namespace std;

void KDTree::Clear()
{
	m_nodes.clear();
	m_balanced.clear();
}

void KDTree::Store(const Vector& point, const Photon& photon)
{
	m_nodes.push_back(Node(point, photon));
}

int KDTree::Find(const Vector& p, const float radius, list<const Node*>* nodes) const
{
	if (nodes)
	{
		Find(p, 1, radius, *nodes);
		return nodes->size();
	}
	else
	{
		list<const Node*> local_nodes;
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

void KDTree::Find(const Vector& point, int nb_elements, vector<const Node*>& nodes, float &max_distance) const
{
	nodes.clear();
	max_distance = numeric_limits<float>::infinity();

	if (m_balanced.empty())
		return;

	nodes.reserve(nb_elements);
	vector<pair<int, float> > dist;
	dist.reserve(nb_elements);

	Find(point, 1, nb_elements, nodes, dist);
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
	}
}
