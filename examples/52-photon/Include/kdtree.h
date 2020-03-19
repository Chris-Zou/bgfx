#pragma once

#include <algorithm>
#include <list>
#include <cmath>
#include <vector>
#include <string>
#include "photon.h"
#include "vector.h"

class Node
{
	friend class KDTree;

public:
	Node();
	Node(const Vector& point, const Photon& photon);
	Vector GetPoint() const;
	Photon GetPhoton() const;

private:
	EDimension	m_axis;
	Vector		m_point;
	Photon		m_photon;
};

class KDTree
{
public:
	KDTree() {};
	void Clear();
	void Store(const Vector& point, const Photon& photon);
	int Find(const Vector&p, const float radius, std::list<const Node*>* nodes) const;
	void Find(const Vector&p, int nb_elements, std::vector<const Node*>& nodes, float &max_distance) const;
	const Node& Find(const Vector&p) const;
	void Balance();
	int Size() const;
	bool IsEmpty() const;
	const Node& operator[](const unsigned int idx) const;
	void DumpToFile(const std::string& filename);
private:
	std::list<Node> m_nodes;
	std::vector<Node> m_balanced;

	static void MedianSplit(std::vector<Node>& p, const int start, const int end, const int median, const EDimension axis);
	static void BalanceSegment(std::vector<Node>& pbal, std::vector<Node>& porg, int index, int start, int end, const Vector& bbmin, const Vector& bbmax);
	unsigned int Closest(const Vector& p, int index, int best) const;
	void Find(const Vector& p, int index, float radius, std::list<const Node*> &nodes) const;
	void Find(const Vector& p, int index, int nb_elements, float &dist_worst, std::vector<const Node*> &nodes, std::vector<std::pair<unsigned int, float> >&dist) const;

	class HeapComparison
	{
	public:
		bool operator()(const std::pair<int, float>& lval, const std::pair<int, float>& rval)
		{
			return lval.second < rval.second;
		}
	};

	void UpdateHeapNodes(const Node& node, const float distance, int nb_elements, std::vector<const Node*>& nodes, std::vector<std::pair<int, float> >& dist) const;
};
