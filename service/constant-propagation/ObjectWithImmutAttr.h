/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <vector>

#include "DisjointUnionAbstractDomain.h"
#include "SignedConstantDomain.h"

#include "TypeUtil.h"

class DexField;
class DexMethod;

using StringDomain = sparta::ConstantAbstractDomain<const DexString*>;
using ConstantClassObjectDomain =
    sparta::ConstantAbstractDomain<const DexType*>;
using AttrDomain =
    sparta::DisjointUnionAbstractDomain<SignedConstantDomain,
                                        StringDomain,
                                        ConstantClassObjectDomain>;

/**
 * Object with immutable primitive attributes.
 *
 * For instance, enum objects may have other non final instance fields, but they
 * always have constant ordinal and name. Boxed integers are
 * constant. Another instance is type-tag field that's generated by
 * Class Merging.
 *
 * #clang-format off
 * an_enum_object {
 *    `Ljava/lang/Enum;.ordinal:()I` return an int constant.
 *    `Ljava/lang/Enum;.name:()Ljava/lang/String;` return a string constant.
 * }
 *
 * a_boxed_integer_object {
 *    `Ljava/lang/Integer;.intValue:()I` return an int constant.
 * }
 *
 * a_class_merging_shape_object {
 *  final int type_tag;  // is an immutable field.
 * }
 * #clang-format off
 */
struct ImmutableAttr {
  struct Attr {
    enum Kind { Method, Field } kind;
    union {
      DexField* field;
      DexMethod* method;
      void* member; // Debug only.
    };
    explicit Attr(DexField* f) : kind(Field), field(f) {
      always_assert(!field->is_def() || (!is_static(field) && is_final(field)));
    }
    explicit Attr(DexMethod* m) : kind(Method), method(m) {
      if (method->is_def()) {
        always_assert(!is_static(method) && !is_constructor(method));
      }
    }

    bool is_method() const { return kind == Method; }
    bool is_field() const { return kind == Field; }
  } attr;
  AttrDomain value;

  ImmutableAttr(const Attr& attr, const SignedConstantDomain& value)
      : attr(attr), value(value) {}

  ImmutableAttr(const Attr& attr, const StringDomain& value)
      : attr(attr), value(value) {}

  ImmutableAttr(const Attr& attr, const ConstantClassObjectDomain& value)
      : attr(attr), value(value) {}
};

struct ObjectWithImmutAttr {
  std::vector<ImmutableAttr> attributes;

  ObjectWithImmutAttr() {}

  /**
   * We just return false and assume that objects are different.
   */
  bool operator==(const ObjectWithImmutAttr&) const { return false; }

  template <typename ValueType>
  void write_value(const ImmutableAttr::Attr& attr, ValueType value) {
#ifndef NDEBUG
    for (auto& att : attributes) {
      always_assert_log(attr.member != att.attr.member,
                        "%s is written before, is it real final attribute?",
                        [&att]() {
                          if (att.attr.is_method()) {
                            return show(att.attr.method);
                          } else {
                            return show(att.attr.field);
                          }
                        }()
                            .c_str());
    }
#endif
    attributes.push_back(ImmutableAttr(attr, value));
  }

  bool empty() const { return attributes.empty(); }

  boost::optional<const AttrDomain> get_value(const DexMethod* method) const {
    for (const auto& attr : attributes) {
      if (attr.attr.is_method() && attr.attr.method == method) {
        return attr.value;
      }
    }
    return boost::none;
  }

  boost::optional<const AttrDomain> get_value(const DexField* field) const {
    for (const auto& attr : attributes) {
      if (attr.attr.is_field() && attr.attr.field == field) {
        return attr.value;
      }
    }
    return boost::none;
  }
};
