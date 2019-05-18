/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Configurable.h"

#include "DexClass.h"

#define error_or_warn(error, warn, msg, ...) \
  always_assert_log(!(error), msg, ##__VA_ARGS__); \
  if (!(warn)) { fprintf(stderr, msg, ##__VA_ARGS__); }

void Configurable::parse_config(const JsonWrapper& json) {
  m_after_configuration = {};
  m_reflector = [](const std::string& param_name, const std::string& param_doc,
                   std::tuple<std::string, ConfigurableReflection,
                              ConfigurableReflection::Type> param_type) {};
  m_parser = [&json](const std::string& name) {
    // TODO: add std::string API for contains
    if (json.contains(name.c_str())) {
      return boost::optional<const Json::Value&>(json[name.c_str()]);
    } else {
      return boost::optional<const Json::Value&>{};
    }
  };
  bind_config();
  // m_after_configuration may have been set in bind_config()
  if (m_after_configuration) {
    m_after_configuration();
  }
}

ConfigurableReflection Configurable::reflect() {
  ConfigurableReflection cr;
  cr.name = get_config_name();
  cr.doc = get_config_doc();
  m_after_configuration = {};
  m_parser = [](const std::string&) {
    return boost::optional<const Json::Value&>{};
  };
  // N.B. using std::tuple here, meant to evolve to use of std::variant w/ c++17
  m_reflector = [&cr](const std::string& param_name,
                      const std::string& param_doc,
                      std::tuple<std::string, ConfigurableReflection,
                                 ConfigurableReflection::Type> param_type) {
    switch (std::get<2>(param_type)) {
    case ConfigurableReflection::Type::PRIMITIVE:
      cr.params[param_name] =
          std::make_tuple(std::get<0>(param_type), ConfigurableReflection(),
                          ConfigurableReflection::Type::PRIMITIVE, param_doc);
      break;
    case ConfigurableReflection::Type::COMPOSITE:
      cr.params[param_name] =
          std::make_tuple("", std::get<1>(param_type),
                          ConfigurableReflection::Type::COMPOSITE, param_doc);
      break;
    default:
      always_assert_log(false, "Invalid ConfigurableReflection::Type: %d",
                        std::get<2>(param_type));
      break;
    }
  };
  bind_config();
  return cr;
}

#define ASSERT_NO_BINDFLAGS(type) \
  always_assert_log(!bindflags, "No bindflags may be specified for a " #type);

template <>
float Configurable::as<float>(const Json::Value& value, bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(float);
  return value.asFloat();
}

template <>
int Configurable::as<int>(const Json::Value& value,
                                  bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(int);
  return value.asInt();
}

template <>
unsigned int Configurable::as<unsigned int>(const Json::Value& value,
                                    bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(unsigned int);
  return value.asUInt();
}

template <>
long Configurable::as<long>(const Json::Value& value,
                                  bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(long);
  return value.asInt64();
}

template <>
unsigned long Configurable::as<unsigned long>(const Json::Value& value,
                                    bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(unsigned long);
  return value.asUInt64();
}

template <>
long long Configurable::as<long long>(const Json::Value& value,
                                  bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(long long);
  return value.asInt64();
}

template <>
unsigned long long Configurable::as<unsigned long long>(const Json::Value& value,
                                    bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(unsigned long long);
  return value.asUInt64();
}

template <>
bool Configurable::as<bool>(const Json::Value& value, bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(bool);
  return value.asBool();
}

template <>
std::string Configurable::as<std::string>(const Json::Value& value,
                                          bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(std::string);
  return value.asString();
}

template <>
boost::optional<std::string> Configurable::as<boost::optional<std::string>>(
    const Json::Value& value, bindflags_t bindflags) {
  always_assert_log(
      !(bindflags & ~Configurable::bindflags::optionals::skip_empty_string),
      "Only bindflags::optionals::skip_empty_string may be specified for a "
      "boost::optional<std::string>");
  std::string str = value.asString();
  if (str.empty() &&
      (bindflags & Configurable::bindflags::optionals::skip_empty_string)) {
    return boost::none;
  } else {
    return str;
  }
}

template <>
std::vector<std::string> Configurable::as<std::vector<std::string>>(
    const Json::Value& value, bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(std::vector<std::string>);
  std::vector<std::string> result;
  for (auto& str : value) {
    result.emplace_back(str.asString());
  }
  return result;
}

template <>
std::unordered_set<std::string>
Configurable::as<std::unordered_set<std::string>>(const Json::Value& value,
                                                  bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(std::unordered_set<std::string>);
  std::unordered_set<std::string> result;
  for (auto& str : value) {
    result.emplace(str.asString());
  }
  return result;
}

template <>
std::vector<DexType*> Configurable::as<std::vector<DexType*>>(
    const Json::Value& value, bindflags_t bindflags) {
  always_assert_log(!(bindflags & ~Configurable::bindflags::types::mask),
             "Only type bindflags may be specified for a "
             "std::vector<DexType*>");
  std::vector<DexType*> result;
  for (auto& str : value) {
    auto type = DexType::get_type(str.asString());
    if (type == nullptr) {
      error_or_warn(
        bindflags & Configurable::bindflags::types::error_if_unresolvable,
        bindflags & Configurable::bindflags::types::warn_if_unresolvable,
        "\"%s\" failed to resolve to a known type", str.asString().c_str()
      );
    } else {
      result.emplace_back(static_cast<DexType*>(type));
    }
  }
  return result;
}

template <>
std::unordered_set<DexType*> Configurable::as<std::unordered_set<DexType*>>(
    const Json::Value& value, bindflags_t bindflags) {
  always_assert_log(!(bindflags & ~Configurable::bindflags::types::mask),
             "Only type bindflags may be specified for a "
             "std::unordered_set<DexType*>, you specified 0x%08x",
             bindflags);
  std::unordered_set<DexType*> result;
  for (auto& str : value) {
    auto type = DexType::get_type(str.asString());
    if (type == nullptr) {
      error_or_warn(
        bindflags & Configurable::bindflags::types::error_if_unresolvable,
        bindflags & Configurable::bindflags::types::warn_if_unresolvable,
        "\"%s\" failed to resolve to a known type", str.asString().c_str()
      );
    } else {
      result.emplace(static_cast<DexType*>(type));
    }
  }
  return result;
}

template <>
std::unordered_set<const DexType*>
Configurable::as<std::unordered_set<const DexType*>>(const Json::Value& value,
                                                     bindflags_t bindflags) {
  always_assert_log(!(bindflags & ~Configurable::bindflags::types::mask),
             "Only type bindflags may be specified for a "
             "std::unordered_set<DexType*>");
  std::unordered_set<const DexType*> result;
  for (auto& str : value) {
    auto type = DexType::get_type(str.asString());
    if (type == nullptr) {
      error_or_warn(
        bindflags & Configurable::bindflags::types::error_if_unresolvable,
        bindflags & Configurable::bindflags::types::warn_if_unresolvable,
        "\"%s\" failed to resolve to a known type", str.asString().c_str()
      );
    } else {
      result.emplace(static_cast<DexType*>(type));
    }
  }
  return result;
}

template <>
std::unordered_set<DexClass*> Configurable::as<std::unordered_set<DexClass*>>(
    const Json::Value& value, bindflags_t bindflags) {
  always_assert_log(!(bindflags & ~Configurable::bindflags::classes::mask),
             "Only type bindflags may be specified for a "
             "std::unordered_set<DexClass*>");
  std::unordered_set<DexClass*> result;
  for (auto& str : value) {
    auto cls =
        type_class(DexType::get_type(DexString::get_string(str.asString())));
    if (cls == nullptr) {
      error_or_warn(
        bindflags & Configurable::bindflags::classes::error_if_unresolvable,
        bindflags & Configurable::bindflags::classes::warn_if_unresolvable,
        "\"%s\" failed to resolve to a known class", str.asString().c_str()
      );
    } else {
      result.emplace(static_cast<DexClass*>(cls));
    }
  }
  return result;
}

template <>
std::unordered_set<DexMethod*> Configurable::as<std::unordered_set<DexMethod*>>(
    const Json::Value& value, bindflags_t bindflags) {
  always_assert_log(!(bindflags & ~Configurable::bindflags::methods::mask),
             "Only method bindflags may be specified for a "
             "std::unordered_set<DexMethod*>");
  std::unordered_set<DexMethod*> result;
  for (auto& str : value) {
    auto meth = DexMethod::get_method(str.asString());
    if (meth == nullptr) {
      error_or_warn(
        bindflags & Configurable::bindflags::methods::error_if_unresolvable,
        bindflags & Configurable::bindflags::methods::warn_if_unresolvable,
        "\"%s\" failed to resolve to a known method", str.asString().c_str()
      );
    } else {
      if (!meth->is_def()) {
        error_or_warn(
          bindflags & Configurable::bindflags::methods::error_if_not_def,
          bindflags & Configurable::bindflags::methods::warn_if_not_def,
          "\"%s\" resolved to a method reference", str.asString().c_str()
        );
      } else {
        result.emplace(static_cast<DexMethod*>(meth));
      }
    }
  }
  return result;
}

template <>
Configurable::MapOfVectorOfStrings
Configurable::as<Configurable::MapOfVectorOfStrings>(const Json::Value& value,
                                                     bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(Configurable::MapOfVectorOfStrings);
  if (!value.isObject()) {
    throw std::runtime_error("expected object, got:" + value.asString());
  }
  Configurable::MapOfVectorOfStrings result;
  for (auto it = value.begin(); it != value.end(); ++it) {
    auto k = it.key();
    auto v = *it;
    if (!k.isString()) {
      throw std::runtime_error("expected string, got:" + k.asString());
    }
    if (!v.isArray()) {
      throw std::runtime_error("expected array, got:" + v.asString());
    }
    for (auto el : v) {
      if (!el.isString()) {
        throw std::runtime_error("expected string, got:" + el.asString());
      }
      result[k.asString()].emplace_back(el.asString());
    }
  }
  return result;
}

template <>
Json::Value Configurable::as<Json::Value>(const Json::Value& value,
                                          bindflags_t bindflags) {
  ASSERT_NO_BINDFLAGS(Json::Value);
  return value;
}

#define IMPLEMENT_REFLECTOR(type)                                             \
  template <>                                                                 \
  void Configurable::reflect(                                                 \
      std::function<void(                                                     \
          (const std::string& param_name, const std::string& param_doc,       \
           std::tuple<std::string, ConfigurableReflection,                    \
                      ConfigurableReflection::Type> param_type))>& reflector, \
      const std::string& name, const std::string& doc, type&) {               \
    reflector(name, doc,                                                      \
              std::make_tuple(std::string{#type}, ConfigurableReflection(),   \
                              ConfigurableReflection::Type::PRIMITIVE));      \
  }

#define IMPLEMENT_REFLECTOR_EX(type, type_name)                               \
  template <>                                                                 \
  void Configurable::reflect(                                                 \
      std::function<void(                                                     \
          (const std::string& param_name, const std::string& param_doc,       \
           std::tuple<std::string, ConfigurableReflection,                    \
                      ConfigurableReflection::Type> param_type))>& reflector, \
      const std::string& name, const std::string& doc, type&) {               \
    reflector(name, doc,                                                      \
              std::make_tuple(std::string{type_name},                         \
                              ConfigurableReflection(),                       \
                              ConfigurableReflection::Type::PRIMITIVE));      \
  }

IMPLEMENT_REFLECTOR(float)
IMPLEMENT_REFLECTOR(bool)
IMPLEMENT_REFLECTOR_EX(int, "int")
IMPLEMENT_REFLECTOR_EX(unsigned int, "int")
IMPLEMENT_REFLECTOR_EX(long, "long")
IMPLEMENT_REFLECTOR_EX(unsigned long, "long")
IMPLEMENT_REFLECTOR_EX(long long, "long")
IMPLEMENT_REFLECTOR_EX(unsigned long long, "long")
IMPLEMENT_REFLECTOR_EX(std::string, "string")
IMPLEMENT_REFLECTOR_EX(Json::Value, "json")
IMPLEMENT_REFLECTOR_EX(boost::optional<std::string>, "string")
IMPLEMENT_REFLECTOR_EX(std::vector<std::string>, "list")
IMPLEMENT_REFLECTOR_EX(std::unordered_set<std::string>, "set")
IMPLEMENT_REFLECTOR_EX(std::vector<DexType*>, "list")
IMPLEMENT_REFLECTOR_EX(std::unordered_set<const DexType*>, "set")
IMPLEMENT_REFLECTOR_EX(std::unordered_set<DexType*>, "set")
IMPLEMENT_REFLECTOR_EX(std::unordered_set<DexClass*>, "set")
IMPLEMENT_REFLECTOR_EX(std::unordered_set<DexMethod*>, "set")
IMPLEMENT_REFLECTOR_EX(Configurable::MapOfVectorOfStrings, "dict")

#undef error_or_warn