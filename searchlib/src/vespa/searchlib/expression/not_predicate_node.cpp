// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include "not_predicate_node.h"
#include "resultnode.h"
#include "resultvector.h"

namespace search::expression {

using vespalib::Deserializer;
using vespalib::Serializer;

IMPLEMENT_IDENTIFIABLE_NS2(search, expression, NotPredicateNode, FilterPredicateNode);

void RegexPredicateNode::RE::compile() {
    regex = Regex::from_pattern(pattern, Regex::Options::None);
}

bool RegexPredicateNode::check(const ResultNode* result) const {
    if (result->inherits(ResultNodeVector::classId)) {
        const auto * rv = static_cast<const ResultNodeVector *>(result);
        for (size_t i = 0; i < rv->size(); i++) {
            HoldString tmp(*rv, i);
            if (_re.regex.full_match(tmp)) return true;
        }
        return false;
    } else {
        HoldString tmp(*result);
        return _re.regex.full_match(tmp);
    }
}

bool RegexPredicateNode::allow(const document::Document & doc, HitRank rank) {
    if (_argument.getRoot()) {
        _argument.execute(doc, rank);
        return check(_argument.getResult());
    }
    return false;
}

bool RegexPredicateNode::allow(DocId docId, HitRank rank) {
    if (_argument.getRoot()) {
        _argument.execute(docId, rank);
        return check(_argument.getResult());
    }
    return false;
}

RegexPredicateNode::RegexPredicateNode() noexcept : _re(), _argument() {}

RegexPredicateNode::~RegexPredicateNode() = default;

RegexPredicateNode::RegexPredicateNode(std::string regex, ExpressionNode::UP input)
  : _re(),
    _argument(std::move(input))
{
    _re.pattern = regex;
    _re.compile();
}


Serializer& RegexPredicateNode::onSerialize(Serializer& os) const { return os << _re.pattern << _argument; }

Deserializer& RegexPredicateNode::onDeserialize(Deserializer& is) {
    is >> _re.pattern;
    _re.compile();
    is >> _argument;
    return is;
}

void RegexPredicateNode::visitMembers(vespalib::ObjectVisitor& visitor) const {
    visit(visitor, "regexp", _re.pattern);
    visit(visitor, "argument", _argument);
}

void RegexPredicateNode::selectMembers(const vespalib::ObjectPredicate& predicate,
                                       vespalib::ObjectOperation& operation) {
    _argument.select(predicate, operation);
}

} // namespace search::expression
