// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
// @author Vegard Sjonfjell

#include "testandsethelper.h"
#include "persistenceutil.h"
#include "fieldvisitor.h"
#include <vespa/persistence/spi/persistenceprovider.h>
#include <vespa/document/base/exceptions.h>
#include <vespa/document/select/parser.h>
#include <vespa/document/repo/documenttyperepo.h>
#include <vespa/vespalib/util/stringfmt.h>

using namespace std::string_literals;

namespace storage {

void TestAndSetHelper::resolveDocumentType(const document::DocumentTypeRepo & documentTypeRepo) {
    if (_doc_type_ptr != nullptr) {
        return;
    }
    if (!_doc_id.hasDocType()) {
        throw TestAndSetException(api::ReturnCode(api::ReturnCode::ILLEGAL_PARAMETERS, "Document id has no doctype"));
    }

    _doc_type_ptr = documentTypeRepo.getDocumentType(_doc_id.getDocType());
    if (_doc_type_ptr == nullptr) {
        throw TestAndSetException(api::ReturnCode(api::ReturnCode::ILLEGAL_PARAMETERS, "Document type does not exist"));
    }
}

void TestAndSetHelper::parseDocumentSelection(const document::DocumentTypeRepo & documentTypeRepo,
                                              const document::BucketIdFactory & bucketIdFactory) {
    document::select::Parser parser(documentTypeRepo, bucketIdFactory);

    try {
        _doc_selection_up = parser.parse(_cmd.getCondition().getSelection());
    } catch (const document::select::ParsingFailedException & e) {
        throw TestAndSetException(api::ReturnCode(api::ReturnCode::ILLEGAL_PARAMETERS, "Failed to parse test and set condition: "s + e.getMessage()));
    }
}

spi::GetResult TestAndSetHelper::retrieveDocument(const document::FieldSet & fieldSet, spi::Context & context) {
    return _spi.get(_env.getBucket(_doc_id, _cmd.getBucket()), fieldSet, _cmd.getDocumentId(), context);
}

TestAndSetHelper::TestAndSetHelper(const PersistenceUtil& env,
                                   const spi::PersistenceProvider& spi,
                                   const document::BucketIdFactory& bucket_id_factory,
                                   const api::TestAndSetCommand& cmd,
                                   DocNotFoundPolicy doc_not_found_policy)
    : _env(env),
      _spi(spi),
      _cmd(cmd),
      _doc_id(cmd.getDocumentId()),
      _doc_type_ptr(_cmd.getDocumentType()),
      _doc_not_found_policy(doc_not_found_policy)
{
    const auto& repo = _env.getDocumentTypeRepo();
    resolveDocumentType(repo);
    parseDocumentSelection(repo, bucket_id_factory);
}

TestAndSetHelper::~TestAndSetHelper() = default;

std::optional<api::ReturnCode>
TestAndSetHelper::retrieveAndMatch(spi::Context& context) {
    // Walk document selection tree to build a minimal field set 
    FieldVisitor fieldVisitor(*_doc_type_ptr);
    try {
        _doc_selection_up->visit(fieldVisitor);
    } catch (const document::FieldNotFoundException& e) {
        return api::ReturnCode(api::ReturnCode::ILLEGAL_PARAMETERS,
                               vespalib::make_string("Condition field '%s' could not be found, or is an imported field. "
                                                     "Imported fields are not supported in conditional mutations.",
                                                     e.getFieldName().c_str()));
    }

    auto result = retrieveDocument(fieldVisitor.getFieldSet(), context);

    // If document exists, match it with selection
    if (result.hasDocument()) {
        auto docPtr = result.getDocumentPtr();
        if (_doc_selection_up->contains(*docPtr) != document::select::Result::True) {
            return api::ReturnCode(api::ReturnCode::TEST_AND_SET_CONDITION_FAILED,
                                   vespalib::make_string("Condition did not match document nodeIndex=%d bucket=%" PRIx64 " %s",
                                                         _env._nodeIndex, _cmd.getBucketId().getRawId(),
                                                         _cmd.hasBeenRemapped() ? "remapped" : ""));
        }

        // Document matches
        return api::ReturnCode();
    } else if (_doc_not_found_policy == DocNotFoundPolicy::TreatAsMatch) {
        return api::ReturnCode();
    } else if (_doc_not_found_policy == DocNotFoundPolicy::ReturnTaSError) {
        return api::ReturnCode(api::ReturnCode::TEST_AND_SET_CONDITION_FAILED,
                               vespalib::make_string("Document does not exist nodeIndex=%d bucket=%" PRIx64 " %s",
                                                     _env._nodeIndex, _cmd.getBucketId().getRawId(),
                                                     _cmd.hasBeenRemapped() ? "remapped" : ""));
    }
    return std::nullopt; // Not found
}

} // storage
