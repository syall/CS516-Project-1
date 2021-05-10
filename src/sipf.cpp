#include <set>
#include <queue>
#include <vector>
#include <tuple>
#include <iostream>
#include <algorithm>
#include <limits>
#include <numeric>
#include "GateNode.hpp"
using namespace std;

pair<vector<int>, string>
sipf(
    set<GateNode *> &frontier,
    set<pair<int, int>> &couplings,
    int num_logical_qubits,
    int num_physical_qubits,
    vector<vector<int>> live_ranges,
    vector<GateNode*> gates_circuit);

static vector<vector<set<int>>>
create_query_graphs(
    set<GateNode *> &frontier,
    int num_logical_qubits);

static vector<set<int>>
create_data_graph(
    set<pair<int, int>> &couplings,
    int num_physical_qubits,
    set<int> filter_qubits);

static pair<vector<set<int>>, pair<int, vector<set<int>>>>
create_dag(
    const vector<set<int>> &query_graph,
    const vector<set<int>> &data_graph);

static pair<vector<set<int>>, vector<set<int>>>
create_candidate_space(
    vector<set<int>> &query_graph,
    vector<set<int>> &candidate_sets,
    vector<set<int>> &query_dag,
    vector<set<int>> &data_graph);

static bool
backtrack_level(
    vector<vector<set<int>>> &query_graphs,
    set<pair<int, int>> &couplings,
    vector<int> &mapping,
    set<int> &seen,
    set<int> &mapped,
    int num_physical_qubits);

static bool
backtrack_level_helper(
    vector<vector<set<int>>> &query_graphs,
    set<pair<int, int>> &couplings,
    vector<set<int>> &candidate_sets,
    vector<set<int>> &candidate_edges,
    vector<set<int>> &parents,
    vector<set<int>> &query_dag,
    vector<int> &mapping,
    set<int> &frontier,
    set<int> &seen,
    set<int> &mapped,
    int num_physical_qubits);

void destroyDependencyGraph(set<GateNode *> &firstGates);

/**
 * Partitioning of Control Relations (PCR)
 * @param frontier Input: Set of Next Candidates
 * @param couplings Input: Coupling Graph
 * @param num_logical_qubits Input: Number of Logical Qubits to be mapped
 * @param num_physical_qubits Input: Maximum number of Physical Qubits to be mapped
 * @return mapping of logical to physical qubits if possible, otherwise NULL
 */
pair<vector<int>, string>
sipf(
    set<GateNode *> &frontier,
    set<pair<int, int>> &couplings,
    int num_logical_qubits,
    int num_physical_qubits,
    vector<vector<int>> live_ranges,
    vector<GateNode*> gates_circuit)
{
    // Input: query graphs
    vector<vector<set<int>>> logical_islands = create_query_graphs(
        frontier,
        num_logical_qubits);
    /*
    for (unsigned int i = 0; i < logical_islands.size(); i++)
    {
        cout << "Logical Island:" << endl;
        vector<set<int>> logical_island = logical_islands[i];
        for (unsigned int q = 0; q < logical_island.size(); q++)
        {
            cout << q << ":";
            for (int neighbor : logical_island[q])
            {
                cout << " " << neighbor;
            }
            cout << endl;
        }
    }
    */

    // M <- EMPTY
    vector<vector<int>> mappings;
    set<int> seen;
    set<int> mapped;
    vector<int> mapping(num_logical_qubits, UNDEFINED_QUBIT);
    if (backtrack_level(
        logical_islands,
        couplings,
        mapping,
        seen,
        mapped,
        num_physical_qubits))
    {
        /*
        cout << "//Location of qubits: ";
        for (unsigned int logical_qubit = 0; logical_qubit < num_logical_qubits; logical_qubit++)
        {
            cout << mapping[logical_qubit];
            if (logical_qubit != num_logical_qubits - 1) {
                cout << ",";
            }
        }
        cout << endl;
        cout << "Seen Logical Qubits:";
        for(auto v : seen) {
            cout << " " << v;
        }
        cout << endl;
        cout << "Mapped Physical Qubits:";
        for(auto v : mapped) {
            cout << " " << v;
        }
        cout << endl;
        */
    }
    else
    {
        // cout << "no" << endl;
        // cout << mapping.size() << endl;
        mapping.clear();
    }

    return pair<vector<int>, string>(mapping, "");

}

/**
 * Create Logical Graph from Dependency Graph
 * @param frontier Input: First Set of Candidates
 * @param num_logical_qubits Input: Number of Logical Qubits
 * @returns graph where indices are vertices and values are adjacent vertices
 */
static vector<vector<set<int>>>
create_query_graphs(
    set<GateNode *> &frontier,
    int num_logical_qubits)
{
    vector<set<int>> logical_graph(num_logical_qubits);

    // BFS all Relations
    {
        queue<GateNode *> search;
        set<GateNode *> seen;
        for (auto gate : frontier)
        {
            search.push(gate);
            seen.insert(gate);
        }
        while (!search.empty())
        {
            GateNode *current = search.front();
            search.pop();

            // Search on Gate
            // If Gate is target only
            if (current->control == UNDEFINED_QUBIT)
            {
                logical_graph[current->target].insert(current->target);
                if (current->targetChild != NULL && seen.find(current->targetChild) == seen.end())
                {
                    seen.insert(current->targetChild);
                    search.push(current->targetChild);
                }
            }
            // If Gate is target and control
            else
            {
                logical_graph[current->target].insert(current->control);
                logical_graph[current->control].insert(current->target);
                if (current->targetChild != NULL && seen.find(current->targetChild) == seen.end())
                {
                    seen.insert(current->targetChild);
                    search.push(current->targetChild);
                }
                if (current->controlChild != NULL && seen.find(current->controlChild) == seen.end())
                {
                    seen.insert(current->controlChild);
                    search.push(current->controlChild);
                }
            }
        }
    }

    vector<vector<set<int>>> logical_islands;

    // BFS Islands
    {
        set<int> seen;
        int last_searched = 0;

        while ((int)seen.size() != num_logical_qubits)
        {
            vector<set<int>> logical_island(num_logical_qubits);

            queue<int> search;
            for (; last_searched < num_logical_qubits; last_searched++)
            {
                if (seen.find(last_searched) == seen.end())
                {
                    search.push(last_searched);
                    break;
                }
            }

            while (!search.empty())
            {
                int current = search.front();
                search.pop();
                seen.insert(current);
                for (int neighbor : logical_graph[current])
                {
                    if (seen.find(neighbor) == seen.end())
                    {
                        logical_island[current].insert(neighbor);
                        logical_island[neighbor].insert(current);
                        search.push(neighbor);
                    }
                }
            }

            if (all_of(logical_island.begin(), logical_island.end(), [](set<int> s) {
                return s.size() == 0;
            })) {
                logical_island[last_searched].insert(last_searched);
            }

            logical_islands.push_back(logical_island);
        }

    }

    // Sort Islands by Size Decreasing
    sort(logical_islands.begin(), logical_islands.end(), [](vector<set<int>>a, vector<set<int>> b){
        int a_size = 0;
        int b_size = 0;
        for (unsigned int i = 0; i < a.size(); i++)
        {
            a_size += a[i].size();
            b_size += b[i].size();
        }
        return a_size > b_size;
    });

    return logical_islands;
}

/**
 * Create Physical Graph from Coupling Architecture
 * @param couplings Input: Coupling Graph Edges
 * @param num_physical_qubits Input: Number of Physical Qubits
 * @returns graph where indices are vertices and values are adjacent vertices
 */
static vector<set<int>>
create_data_graph(
    set<pair<int, int>> &couplings,
    int num_physical_qubits,
    set<int> filter_qubits)
{
    vector<set<int>> physical_graph(num_physical_qubits);

    // Iterate Edges to form Adjacency Lists
    for (auto edge : couplings)
    {
        if (filter_qubits.find(edge.first) == filter_qubits.end() &&
            filter_qubits.find(edge.second) == filter_qubits.end())
        {
            physical_graph[edge.first].insert(edge.second);
            physical_graph[edge.second].insert(edge.first);
        }
    }

    return physical_graph;
}

static pair<vector<set<int>>, pair<int, vector<set<int>>>>
create_dag(
    const vector<set<int>> &query_graph,
    const vector<set<int>> &data_graph)
{
    vector<set<int>> candidate_sets(query_graph.size());
    pair<int, float> minimum_root(0, numeric_limits<double>::max());

    for (unsigned int root = 0; root < query_graph.size(); root++)
    {
        unsigned int root_degree = query_graph[root].size();
        if (root_degree == 0)
        {
            continue;
        }
        for (unsigned int v = 0; v < data_graph.size(); v++)
        {
            if (data_graph[v].size() >= root_degree)
            {
                candidate_sets[root].insert(v);
            }
        }
        double heuristic = (double)candidate_sets[root].size() / (double)root_degree;
        if (heuristic < minimum_root.second)
        {
            minimum_root.first = root;
            minimum_root.second = heuristic;
        }
    }

    // BFS
    vector<set<int>> minimum_root_dag(query_graph.size());
    queue<int> search;
    search.push(minimum_root.first);
    set<int> seen;
    while (!search.empty())
    {
        int current = search.front();
        search.pop();
        seen.insert(current);
        for (int neighbor : query_graph[current])
        {
            if (seen.find(neighbor) == seen.end())
            {
                minimum_root_dag[current].insert(neighbor);
                search.push(neighbor);
            }
        }
    }

    return pair<vector<set<int>>, pair<int, vector<set<int>>>>(
        candidate_sets,
        pair<int, vector<set<int>>>(minimum_root.first, minimum_root_dag));
}

static pair<vector<set<int>>, vector<set<int>>>
create_candidate_space(
    vector<set<int>> &query_graph,
    vector<set<int>> &candidate_sets,
    vector<set<int>> &query_dag,
    vector<set<int>> &data_graph)
{
    // Candidate Space = Sets(query -> set<data>), Edges(data -> set<data>)
    vector<set<int>> candidate_edges(data_graph.size());
    for (unsigned int u1 = 0; u1 < candidate_sets.size(); u1++)
    {
        for (unsigned int u2 = 0; u2 < candidate_sets.size(); u2++)
        {
            // There is an edge between q1 and q2 in the query graph
            if (query_graph[u1].find(u2) == query_graph[u1].end())
            {
                continue;
            }
            // Where d1 and d2 are data graph vertices in candidate sets of u1 and u2
            for (int d1 : candidate_sets[u1])
            {
                for (int d2 : candidate_sets[u2])
                {
                    // There is an edge between d1 and d2 in the query graph
                    if (data_graph[d1].find(d2) != data_graph[d1].end())
                    {
                        candidate_edges[d1].insert(d2);
                        candidate_edges[d2].insert(d1);
                    }
                }
            }
        }
    }

    return pair<vector<set<int>>, vector<set<int>>>(candidate_sets, candidate_edges);
}

static bool
backtrack_level(
    vector<vector<set<int>>> &query_graphs,
    set<pair<int, int>> &couplings,
    vector<int> &mapping,
    set<int> &seen,
    set<int> &mapped,
    int num_physical_qubits)
{

    if (query_graphs.empty())
    {
        return true;
    }

    vector<set<int>> logical_graph = *query_graphs.begin();

    // Input: query graph q
    int logical_size = 0;
    for_each(logical_graph.begin(), logical_graph.end(), [&logical_size](set<int> s) {
        logical_size += s.size();
    });
    /*
    cout << "Logical Graph of size " << logical_size << ":" << endl;
    for (unsigned int q = 0; q < logical_graph.size(); q++)
    {
        cout << q << ":";
        for (int neighbor : logical_graph[q])
        {
            cout << " " << neighbor;
        }
        cout << endl;
    }
    */

    // Input: data graph G
    vector<set<int>> physical_graph = create_data_graph(couplings, num_physical_qubits, mapped);
    /*
    cout << "Seen Logical Qubits:";
    for(auto v : seen) {
        cout << " " << v;
    }
    cout << endl;
    cout << "Mapped Physical Qubits:";
    for(auto v : mapped) {
        cout << " " << v;
    }
    cout << endl;
    cout << "Physical Graph:" << endl;
    for (unsigned int q = 0; q < physical_graph.size(); q++)
    {
        cout << q << ":";
        for (int neighbor : physical_graph[q])
        {
            cout << " " << neighbor;
        }
        cout << endl;
    }
    */

    // Backtrack(q, q_D, CS, M)
    vector<vector<set<int>>> next_level = query_graphs;
    next_level.erase(next_level.begin());

    if (logical_size == 1)
    {
        int logical_qubit = UNDEFINED_QUBIT;
        for (unsigned int q = 0; q < logical_graph.size(); q++)
        {
            if (logical_graph[q].size() == 1)
            {
                logical_qubit = (int)q;
                break;
            }
        }

        if (logical_qubit == UNDEFINED_QUBIT)
        {
            return false;
        }

        int physical_qubit = UNDEFINED_QUBIT;
        for (int q = 0; q < num_physical_qubits; q++)
        {
            if (mapped.find(q) == mapped.end())
            {
                physical_qubit = q;
                break;
            }
        }

        if (physical_qubit == UNDEFINED_QUBIT)
        {
            return false;
        }

        seen.insert(logical_qubit);
        mapped.insert(physical_qubit);
        mapping[logical_qubit] = physical_qubit;

        return backtrack_level(
            next_level,
            couplings,
            mapping,
            seen,
            mapped,
            num_physical_qubits);
    }
    else
    {

        // q_D <- BuildDAG(q, G)
        pair<vector<set<int>>, pair<int, vector<set<int>>>> logical_dag_result = create_dag(
            logical_graph,
            physical_graph);
        vector<set<int>> candidate_sets = logical_dag_result.first;
        int dag_root = logical_dag_result.second.first;
        vector<set<int>> logical_dag = logical_dag_result.second.second;
        /*
        cout << "Logical DAG with root " << dag_root << ":" << endl;
        for (unsigned int q = 0; q < logical_dag.size(); q++)
        {
            cout << q << ":";
            for (int neighbor : logical_dag[q])
            {
                cout << " " << neighbor;
            }
            cout << endl;
        }
        */

        // CS <- BuildCS(q, q_D, G)
        pair<vector<set<int>>, vector<set<int>>> candidate_space = create_candidate_space(
            logical_graph,
            candidate_sets,
            logical_dag,
            physical_graph);
        vector<set<int>> candidate_edges = candidate_space.second;
        /*
        cout << "Candidate Sets:" << endl;
        for (unsigned int q = 0; q < candidate_sets.size(); q++)
        {
            cout << q << ":";
            for (int candidate : candidate_sets[q])
            {
                cout << " " << candidate;
            }
            cout << endl;
        }
        cout << "Candidate Edges:" << endl;
        for (unsigned int q = 0; q < candidate_edges.size(); q++)
        {
            cout << q << ":";
            for (int neighbor : candidate_edges[q])
            {
                cout << " " << neighbor;
            }
            cout << endl;
        }
        */

        // Parents
        vector<set<int>> parents(logical_dag.size());
        for (unsigned int v = 0; v < logical_dag.size(); v++)
        {
            for (int neighbor : logical_dag[v])
            {
                parents[neighbor].insert(v);
            }
        }
        /*
        cout << "Parents:" << endl;
        for (unsigned int q = 0; q < parents.size(); q++)
        {
            cout << q << ":";
            for (int neighbor : parents[q])
            {
                cout << " " << neighbor;
            }
            cout << endl;
        }
        */

        set<int> frontier{dag_root};
        return backtrack_level_helper(
            next_level,
            couplings,
            candidate_sets,
            candidate_edges,
            parents,
            logical_dag,
            mapping,
            frontier,
            seen,
            mapped,
            num_physical_qubits);
    }
}

static bool
backtrack_level_helper(
    vector<vector<set<int>>> &query_graphs,
    set<pair<int, int>> &couplings,
    vector<set<int>> &candidate_sets,
    vector<set<int>> &candidate_edges,
    vector<set<int>> &parents,
    vector<set<int>> &query_dag,
    vector<int> &mapping,
    set<int> &frontier,
    set<int> &seen,
    set<int> &mapped,
    int num_physical_qubits)
{

    // If frontier is empty
    if (frontier.empty())
    {
        // TODO: If no more query graphs, success
        if (query_graphs.empty())
        {
            // cout << "query_graphs is empty" << endl;
            return true;
        }
        // TODO: Otherwise, recursively call with new query graph
        else
        {
            // cout << "query_graphs is not empty" << endl;
            return backtrack_level(
                query_graphs,
                couplings,
                mapping,
                seen,
                mapped,
                num_physical_qubits);
            // return true;
        }
    }

    // Currently ordered by int compare
    // TODO: Adaptive Matching Order
    // TODO: Pruning by Failing Sets
    for (int current : frontier)
    {

        // cout << current << endl;

        // If no parents, aka root
        if (parents[current].empty())
        {
            set<int> new_frontier = frontier;
            new_frontier.erase(current);
            for (int child : query_dag[current])
            {
                new_frontier.insert(child);
            }

            set<int> new_seen = seen;
            new_seen.insert(current);

            vector<int> new_mapping = mapping;

            set<int> new_mapped = mapped;

            for (int candidate : candidate_sets[current])
            {
                new_mapping[current] = candidate;
                new_mapped.insert(candidate);

                if (backtrack_level_helper(
                        query_graphs,
                        couplings,
                        candidate_sets,
                        candidate_edges,
                        parents,
                        query_dag,
                        new_mapping,
                        new_frontier,
                        new_seen,
                        new_mapped,
                        num_physical_qubits))
                {
                    seen = new_seen;
                    mapped = new_mapped;
                    mapping = new_mapping;
                    return true;
                }

                new_mapping[current] = UNDEFINED_QUBIT;
                new_mapped.erase(candidate);
            }

            return false;
        }
        // If parents
        else
        {
            // Extendable if parents are matched
            if (any_of(parents[current].begin(), parents[current].end(),
                       [&seen](int parent) { return seen.find(parent) == seen.end(); }))
            {
                continue;
            }
            // Candidates of current vertex =
            //     Intersection of parents' mapped candidates' adjacency list
            set<int> candidates_list;
            for (int candidate : candidate_sets[current])
            {
                if (mapped.find(candidate) != mapped.end())
                {
                    continue;
                }
                if (all_of(parents[current].begin(), parents[current].end(),
                           [&candidate_edges, &mapping, candidate](int parent) {
                               return (candidate_edges[mapping[parent]].find(candidate) !=
                                       candidate_edges[mapping[parent]].end());
                           }))
                {
                    candidates_list.insert(candidate);
                }
            }

            if (candidates_list.empty())
            {
                // cout << "no more candidates" << endl;
                return false;
            }

            set<int> new_frontier = frontier;
            new_frontier.erase(current);
            for (int child : query_dag[current])
            {
                if (seen.find(child) == seen.end())
                {
                    new_frontier.insert(child);
                }
            }

            set<int> new_seen = seen;
            new_seen.insert(current);

            vector<int> new_mapping = mapping;

            set<int> new_mapped = mapped;

            for (int candidate : candidates_list)
            {
                new_mapping[current] = candidate;
                new_mapped.insert(candidate);

                if (backtrack_level_helper(
                        query_graphs,
                        couplings,
                        candidate_sets,
                        candidate_edges,
                        parents,
                        query_dag,
                        new_mapping,
                        new_frontier,
                        new_seen,
                        new_mapped,
                        num_physical_qubits))
                {
                    seen = new_seen;
                    mapped = new_mapped;
                    mapping = new_mapping;
                    return true;
                }

                new_mapping[current] = UNDEFINED_QUBIT;
                new_mapped.erase(candidate);
            }

            return false;
        }

    }

    return false;

}

/**
 * Destroy all GateNodes in the Dependency Graph
 * @param firstGates Input: Set of First Candidates
 */
void destroyDependencyGraph(set<GateNode *> &firstGates)
{
    // BFS
    queue<GateNode *> search;
    set<GateNode *> seen;
    for (auto gate : firstGates)
    {
        search.push(gate);
        seen.insert(gate);
    }
    while (!search.empty())
    {
        GateNode *current = search.front();
        search.pop();

        // Search on Gate
        // If Gate is target only
        if (current->control == UNDEFINED_QUBIT)
        {
            if (current->targetChild != NULL && seen.find(current->targetChild) == seen.end())
            {
                seen.insert(current->targetChild);
                search.push(current->targetChild);
            }
        }
        // If Gate is target and control
        else
        {
            if (current->targetChild != NULL && seen.find(current->targetChild) == seen.end())
            {
                seen.insert(current->targetChild);
                search.push(current->targetChild);
            }
            if (current->controlChild != NULL && seen.find(current->controlChild) == seen.end())
            {
                seen.insert(current->controlChild);
                search.push(current->controlChild);
            }
        }

        // Delete GateNode
        delete current;
    }
}
