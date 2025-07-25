// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
/**
 * Defines a Group object that defines a group of groups/nodes.
 *
 * The "1|*" partitions representation is stored as an array of double,
 * where the star (*) is represented by zero (0).
 * The subgroups and storagenode/distributor indexes are stored in increasing order.
 *
 */
#pragma once

#include "redundancygroupdistribution.h"
#include <vespa/vespalib/objects/floatingpointtype.h>
#include <vespa/vespalib/util/crc.h>
#include <map>
#include <vector>
#include <memory>

namespace vespalib { class asciistream; }

namespace storage::lib {

class Group : public document::Printable
{
public:
    using UP = std::unique_ptr<Group>;
    using Distribution = RedundancyGroupDistribution;

private:
    std::string                _name;
    uint16_t                   _index;
    uint16_t                   _descendent_node_count;
    uint32_t                   _distributionHash;
    Distribution               _distributionSpec;
    std::vector<Distribution>  _preCalculated;
    vespalib::Double           _capacity;
    std::map<uint16_t, Group*> _subGroups; // Set if branch group
    // Invariant: _nodes is ordered by ascending index value.
    std::vector<uint16_t>      _nodes;     // Set if leaf group
    // Same set of indices as _nodes, but in the order originally given as
    // part of setNodes(), i.e. may not be ordered.
    // TODO(vekterli): this can be removed once model code is guaranteed to
    // output nodes in a well-defined order, i.e. _originalNodes == _nodes.
    std::vector<uint16_t>      _originalNodes;

    void calculateDistributionHashValues(uint32_t parentHash) noexcept;
    [[nodiscard]] uint16_t update_descendent_node_counts() noexcept;
    void getConfigHash(vespalib::asciistream & out) const;

    /**
     * Calculates distribution hashes, used to create unique values for each
     * group to XOR their bucket seeds with. Calculated based on index of itself
     * and parent groups. Call this on the root group to generate all hashes.
     */
    void calculateDistributionHashValues() noexcept {
        calculateDistributionHashValues(0x8badf00d);
    }

public:
    // Create leaf node
    Group(uint16_t index, std::string_view name) noexcept;
    // Create branch node
    Group(uint16_t index, std::string_view name,
          const Distribution&, uint16_t redundancy);
    ~Group() override;

    [[nodiscard]] bool isLeafGroup() const noexcept { return ! _nodes.empty(); }
    // Total number of leaf nodes existing recursively in/under this group
    [[nodiscard]] uint16_t descendent_node_count() const noexcept { return _descendent_node_count; }
    bool operator==(const Group& other) const noexcept;
    void print(std::ostream& out, bool verbose, const std::string& indent) const override;

    vespalib::Double getCapacity() const noexcept { return _capacity; }
    const std::string & getName() const noexcept { return _name; }
    uint16_t getIndex() const noexcept { return _index; }
    std::map<uint16_t, Group*>& getSubGroups() { return _subGroups; }
    const std::map<uint16_t, Group*>& getSubGroups() const noexcept { return _subGroups; }
    const std::vector<uint16_t>& getNodes() const noexcept { return _nodes; };
    const Distribution& getDistributionSpec() const noexcept { return _distributionSpec; }
    [[nodiscard]] bool redundancy_value_within_bounds(size_t idx) const noexcept { return idx < _preCalculated.size(); }
    const Distribution& getDistribution(uint16_t redundancy) const noexcept { return _preCalculated[redundancy]; }
    uint32_t getDistributionHash() const noexcept { return _distributionHash; }

    void addSubGroup(Group::UP);
    void setCapacity(vespalib::Double capacity);
    void setNodes(const std::vector<uint16_t>& nodes);

    /**
     * Returns the hierarchical group the given node is in.
     */
    const Group* getGroupForNode(uint16_t index) const;

    /**
     * Computes certain important immutable properties of the group/node tree.
     * Must always be invoked prior to the first use of any ideal state computation.
     */
    void finalize() noexcept;

    /**
     * Get a string uniquely describing the parts of the distribution config
     * that is critical for distribution. Use to match up two different group
     * instances in order to verify if they would generate the same distribution
     */
    std::string getDistributionConfigHash() const;
};

}
