// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

namespace storage {

// Specifies the semantics of a test-and-set comparison where the underlying
// document does not exist in the backing store.
enum class DocNotFoundPolicy {
    ReturnTaSError, // Return an explicit test-and-set failure return code (_not_ a not-found error)
    TreatAsMatch,   // Act as if the document _did_ exist and the condition matched it
    ReturnNotFound  // Explicitly propagate up the fact that the document did not exist
};

}
