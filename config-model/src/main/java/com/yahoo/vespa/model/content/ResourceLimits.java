// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.content;

import com.yahoo.vespa.config.content.FleetcontrollerConfig;
import com.yahoo.vespa.config.search.core.ProtonConfig;

import java.util.Optional;

/**
 * Class tracking feed block resource limits used by a component in a content cluster (e.g. cluster controller or content node).
 *
 * @author geirst
 */
public class ResourceLimits implements FleetcontrollerConfig.Producer, ProtonConfig.Producer {

    private final Optional<Double> diskLimit;
    private final Optional<Double> memoryLimit;
    private final Optional<Double> lowWatermarkDifference;
    private final Optional<Double> addressSpaceLimit;

    private ResourceLimits(Builder builder) {
        this.diskLimit = builder.diskLimit;
        this.memoryLimit = builder.memoryLimit;
        this.lowWatermarkDifference = builder.lowWatermarkDifference;
        this.addressSpaceLimit = builder.addressSpaceLimit;
    }

    public Optional<Double> getDiskLimit() {
        return diskLimit;
    }

    public Optional<Double> getMemoryLimit() {
        return memoryLimit;
    }

    public Optional<Double> getLowWatermarkDifference() {
        return lowWatermarkDifference;
    }

    public Optional<Double> getAddressSpaceLimit() {
        return addressSpaceLimit;
    }

    @Override
    public void getConfig(FleetcontrollerConfig.Builder builder) {
        // Note: The resource categories must match the ones used in host info reporting
        // between content nodes and cluster controller:
        // storage/src/vespa/storage/persistence/filestorage/service_layer_host_info_reporter.cpp
        builder.cluster_feed_block_limit.put("memory", memoryLimit.orElse(0.8));
        builder.cluster_feed_block_limit.put("disk", diskLimit.orElse(0.75));
        builder.cluster_feed_block_limit.put("attribute-address-space", 0.89);
        builder.cluster_feed_block_noise_level(lowWatermarkDifference.orElse(0.0));
    }

    @Override
    public void getConfig(ProtonConfig.Builder builder) {
        diskLimit.ifPresent(d -> builder.writefilter.disklimit(d));
        memoryLimit.ifPresent(d -> builder.writefilter.memorylimit(d));
    }

    public static class Builder {

        private Optional<Double> diskLimit = Optional.empty();
        private Optional<Double> memoryLimit = Optional.empty();
        private Optional<Double> lowWatermarkDifference = Optional.empty();
        private Optional<Double> addressSpaceLimit = Optional.empty();

        public ResourceLimits build() { return new ResourceLimits(this); }

        public Optional<Double> getDiskLimit() { return diskLimit; }

        public Builder setDiskLimit(double diskLimit) {
            this.diskLimit = Optional.of(diskLimit);
            return this;
        }

        public Optional<Double> getMemoryLimit() { return memoryLimit; }

        public Builder setMemoryLimit(double memoryLimit) {
            this.memoryLimit = Optional.of(memoryLimit);
            return this;
        }

        public Optional<Double> getLowWatermarkDifference() { return lowWatermarkDifference; }

        public Builder setLowWatermarkDifference(double lowWatermarkDifference) {
            this.lowWatermarkDifference = Optional.of(lowWatermarkDifference);
            return this;
        }

        public Optional<Double> getAddressSpaceLimit() { return addressSpaceLimit; }

        public Builder setAddressSpaceLimit(double addressSpaceLimit) {
            this.addressSpaceLimit = Optional.of(addressSpaceLimit);
            return this;
        }

    }
}
