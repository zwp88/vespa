// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
// @author Vegard Sjonfjell

#pragma once

#include "doc_not_found_policy.h"
#include <vespa/storageapi/message/persistence.h>
#include <vespa/persistence/spi/result.h>
#include <stdexcept>
#include <optional>

namespace document::select { class Node; }
namespace document {
    class FieldSet;
    class BucketIdFactory;
}

namespace storage {

namespace spi {
    class Context;
    struct PersistenceProvider;
}
class PersistenceThread;
class ServiceLayerComponent;
class PersistenceUtil;

class TestAndSetException : public std::runtime_error {
    api::ReturnCode _code;

public:
    TestAndSetException(api::ReturnCode code)
        : std::runtime_error(code.getMessage()),
        _code(std::move(code))
    {}

    const api::ReturnCode & getCode() const { return _code; }
};

class TestAndSetHelper {
    const PersistenceUtil&                  _env;
    const spi::PersistenceProvider&         _spi;
    const api::TestAndSetCommand&           _cmd;
    const document::DocumentId              _doc_id;
    const document::DocumentType*           _doc_type_ptr;
    std::unique_ptr<document::select::Node> _doc_selection_up;
    DocNotFoundPolicy                       _doc_not_found_policy;

    void resolveDocumentType(const document::DocumentTypeRepo & documentTypeRepo);
    void parseDocumentSelection(const document::DocumentTypeRepo & documentTypeRepo,
                                const document::BucketIdFactory & bucketIdFactory);
    spi::GetResult retrieveDocument(const document::FieldSet & fieldSet, spi::Context & context);

public:
    TestAndSetHelper(const PersistenceUtil& env, const spi::PersistenceProvider& _spi,
                     const document::BucketIdFactory& bucket_id_factory,
                     const api::TestAndSetCommand& cmd,
                     DocNotFoundPolicy doc_not_found_policy = DocNotFoundPolicy::ReturnTaSError);
    ~TestAndSetHelper();
    // If document was not found:
    //   iff doc_not_found_returns_tas_error == true, returns nullopt
    //   otherwise, returns a TaS error return code
    std::optional<api::ReturnCode> retrieveAndMatch(spi::Context& context);
};

} // storage
