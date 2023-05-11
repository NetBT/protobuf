// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_MAP_FIELD_H__
#define GOOGLE_PROTOBUF_MAP_FIELD_H__

#include <atomic>
#include <functional>
#include <type_traits>

#include "google/protobuf/arena.h"
#include "google/protobuf/port.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/map_entry.h"
#include "google/protobuf/map_field_lite.h"
#include "google/protobuf/map_type_handler.h"
#include "google/protobuf/message.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/unknown_field_set.h"


// Must be included last.
#include "google/protobuf/port_def.inc"

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
class DynamicMessage;
class MapIterator;

// Microsoft compiler complains about non-virtual destructor,
// even when the destructor is private.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4265)
#endif  // _MSC_VER

#define TYPE_CHECK(EXPECTEDTYPE, METHOD)                                  \
  if (type() != EXPECTEDTYPE) {                                           \
    ABSL_LOG(FATAL) << "Protocol Buffer map usage error:\n"               \
                    << METHOD << " type does not match\n"                 \
                    << "  Expected : "                                    \
                    << FieldDescriptor::CppTypeName(EXPECTEDTYPE) << "\n" \
                    << "  Actual   : "                                    \
                    << FieldDescriptor::CppTypeName(type());              \
  }

// MapKey is an union type for representing any possible
// map key.
class PROTOBUF_EXPORT MapKey {
 public:
  MapKey() : type_() {}
  MapKey(const MapKey& other) : type_() { CopyFrom(other); }

  MapKey& operator=(const MapKey& other) {
    CopyFrom(other);
    return *this;
  }

  ~MapKey() {
    if (type_ == FieldDescriptor::CPPTYPE_STRING) {
      val_.string_value_.Destruct();
    }
  }

  FieldDescriptor::CppType type() const {
    if (type_ == FieldDescriptor::CppType()) {
      ABSL_LOG(FATAL) << "Protocol Buffer map usage error:\n"
                      << "MapKey::type MapKey is not initialized. "
                      << "Call set methods to initialize MapKey.";
    }
    return type_;
  }

  void SetInt64Value(int64_t value) {
    SetType(FieldDescriptor::CPPTYPE_INT64);
    val_.int64_value_ = value;
  }
  void SetUInt64Value(uint64_t value) {
    SetType(FieldDescriptor::CPPTYPE_UINT64);
    val_.uint64_value_ = value;
  }
  void SetInt32Value(int32_t value) {
    SetType(FieldDescriptor::CPPTYPE_INT32);
    val_.int32_value_ = value;
  }
  void SetUInt32Value(uint32_t value) {
    SetType(FieldDescriptor::CPPTYPE_UINT32);
    val_.uint32_value_ = value;
  }
  void SetBoolValue(bool value) {
    SetType(FieldDescriptor::CPPTYPE_BOOL);
    val_.bool_value_ = value;
  }
  void SetStringValue(std::string val) {
    SetType(FieldDescriptor::CPPTYPE_STRING);
    *val_.string_value_.get_mutable() = std::move(val);
  }

  int64_t GetInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT64, "MapKey::GetInt64Value");
    return val_.int64_value_;
  }
  uint64_t GetUInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT64, "MapKey::GetUInt64Value");
    return val_.uint64_value_;
  }
  int32_t GetInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT32, "MapKey::GetInt32Value");
    return val_.int32_value_;
  }
  uint32_t GetUInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT32, "MapKey::GetUInt32Value");
    return val_.uint32_value_;
  }
  bool GetBoolValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_BOOL, "MapKey::GetBoolValue");
    return val_.bool_value_;
  }
  const std::string& GetStringValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_STRING, "MapKey::GetStringValue");
    return val_.string_value_.get();
  }

  bool operator<(const MapKey& other) const {
    if (type_ != other.type_) {
      // We could define a total order that handles this case, but
      // there currently no need.  So, for now, fail.
      ABSL_LOG(FATAL) << "Unsupported: type mismatch";
    }
    switch (type()) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
      case FieldDescriptor::CPPTYPE_FLOAT:
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        ABSL_LOG(FATAL) << "Unsupported";
        return false;
      case FieldDescriptor::CPPTYPE_STRING:
        return val_.string_value_.get() < other.val_.string_value_.get();
      case FieldDescriptor::CPPTYPE_INT64:
        return val_.int64_value_ < other.val_.int64_value_;
      case FieldDescriptor::CPPTYPE_INT32:
        return val_.int32_value_ < other.val_.int32_value_;
      case FieldDescriptor::CPPTYPE_UINT64:
        return val_.uint64_value_ < other.val_.uint64_value_;
      case FieldDescriptor::CPPTYPE_UINT32:
        return val_.uint32_value_ < other.val_.uint32_value_;
      case FieldDescriptor::CPPTYPE_BOOL:
        return val_.bool_value_ < other.val_.bool_value_;
    }
    return false;
  }

  bool operator==(const MapKey& other) const {
    if (type_ != other.type_) {
      // To be consistent with operator<, we don't allow this either.
      ABSL_LOG(FATAL) << "Unsupported: type mismatch";
    }
    switch (type()) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
      case FieldDescriptor::CPPTYPE_FLOAT:
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        ABSL_LOG(FATAL) << "Unsupported";
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        return val_.string_value_.get() == other.val_.string_value_.get();
      case FieldDescriptor::CPPTYPE_INT64:
        return val_.int64_value_ == other.val_.int64_value_;
      case FieldDescriptor::CPPTYPE_INT32:
        return val_.int32_value_ == other.val_.int32_value_;
      case FieldDescriptor::CPPTYPE_UINT64:
        return val_.uint64_value_ == other.val_.uint64_value_;
      case FieldDescriptor::CPPTYPE_UINT32:
        return val_.uint32_value_ == other.val_.uint32_value_;
      case FieldDescriptor::CPPTYPE_BOOL:
        return val_.bool_value_ == other.val_.bool_value_;
    }
    ABSL_LOG(FATAL) << "Can't get here.";
    return false;
  }

  void CopyFrom(const MapKey& other) {
    SetType(other.type());
    switch (type_) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
      case FieldDescriptor::CPPTYPE_FLOAT:
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        ABSL_LOG(FATAL) << "Unsupported";
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        *val_.string_value_.get_mutable() = other.val_.string_value_.get();
        break;
      case FieldDescriptor::CPPTYPE_INT64:
        val_.int64_value_ = other.val_.int64_value_;
        break;
      case FieldDescriptor::CPPTYPE_INT32:
        val_.int32_value_ = other.val_.int32_value_;
        break;
      case FieldDescriptor::CPPTYPE_UINT64:
        val_.uint64_value_ = other.val_.uint64_value_;
        break;
      case FieldDescriptor::CPPTYPE_UINT32:
        val_.uint32_value_ = other.val_.uint32_value_;
        break;
      case FieldDescriptor::CPPTYPE_BOOL:
        val_.bool_value_ = other.val_.bool_value_;
        break;
    }
  }

 private:
  template <typename K, typename V>
  friend class internal::TypeDefinedMapFieldBase;
  friend class MapIterator;
  friend class internal::DynamicMapField;

  union KeyValue {
    KeyValue() {}
    internal::ExplicitlyConstructed<std::string> string_value_;
    int64_t int64_value_;
    int32_t int32_value_;
    uint64_t uint64_value_;
    uint32_t uint32_value_;
    bool bool_value_;
  } val_;

  void SetType(FieldDescriptor::CppType type) {
    if (type_ == type) return;
    if (type_ == FieldDescriptor::CPPTYPE_STRING) {
      val_.string_value_.Destruct();
    }
    type_ = type;
    if (type_ == FieldDescriptor::CPPTYPE_STRING) {
      val_.string_value_.DefaultConstruct();
    }
  }

  // type_ is 0 or a valid FieldDescriptor::CppType.
  // Use "CppType()" to indicate zero.
  FieldDescriptor::CppType type_;
};

namespace internal {
template <>
struct is_internal_map_key_type<MapKey> : std::true_type {};
}  // namespace internal

}  // namespace protobuf
}  // namespace google
namespace std {
template <>
struct hash<google::protobuf::MapKey> {
  size_t operator()(const google::protobuf::MapKey& map_key) const {
    switch (map_key.type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        ABSL_LOG(FATAL) << "Unsupported";
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        return hash<std::string>()(map_key.GetStringValue());
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
        auto value = map_key.GetInt64Value();
        return hash<decltype(value)>()(value);
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
        auto value = map_key.GetInt32Value();
        return hash<decltype(value)>()(map_key.GetInt32Value());
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
        auto value = map_key.GetUInt64Value();
        return hash<decltype(value)>()(map_key.GetUInt64Value());
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
        auto value = map_key.GetUInt32Value();
        return hash<decltype(value)>()(map_key.GetUInt32Value());
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
        return hash<bool>()(map_key.GetBoolValue());
      }
    }
    ABSL_LOG(FATAL) << "Can't get here.";
    return 0;
  }
  bool operator()(const google::protobuf::MapKey& map_key1,
                  const google::protobuf::MapKey& map_key2) const {
    return map_key1 < map_key2;
  }
};
}  // namespace std

namespace google {
namespace protobuf {
namespace internal {

class ContendedMapCleanTest;
class GeneratedMessageReflection;
class MapFieldAccessor;

// This class provides access to map field using reflection, which is the same
// as those provided for RepeatedPtrField<Message>. It is used for internal
// reflection implementation only. Users should never use this directly.
class PROTOBUF_EXPORT MapFieldBase : public MapFieldBaseForParse {
 public:
  constexpr MapFieldBase() {}
  explicit MapFieldBase(Arena* arena) : payload_{ToTaggedPtr(arena)} {}
  MapFieldBase(const MapFieldBase&) = delete;
  MapFieldBase& operator=(const MapFieldBase&) = delete;

 protected:
  // "protected" stops users from deleting a `MapFieldBase *`
  ~MapFieldBase();

 public:
  // Returns reference to internal repeated field. Data written using
  // Map's api prior to calling this function is guarantted to be
  // included in repeated field.
  const RepeatedPtrFieldBase& GetRepeatedField() const;

  // Like above. Returns mutable pointer to the internal repeated field.
  RepeatedPtrFieldBase* MutableRepeatedField();

  // Pure virtual map APIs for Map Reflection.
  virtual bool ContainsMapKey(const MapKey& map_key) const = 0;
  virtual bool InsertOrLookupMapValue(const MapKey& map_key,
                                      MapValueRef* val) = 0;
  virtual bool LookupMapValue(const MapKey& map_key,
                              MapValueConstRef* val) const = 0;
  bool LookupMapValue(const MapKey&, MapValueRef*) const = delete;

  // Returns whether changes to the map are reflected in the repeated field.
  bool IsRepeatedFieldValid() const;
  // Insures operations after won't get executed before calling this.
  bool IsMapValid() const;
  virtual bool DeleteMapValue(const MapKey& map_key) = 0;
  virtual bool EqualIterator(const MapIterator& a,
                             const MapIterator& b) const = 0;
  virtual void MapBegin(MapIterator* map_iter) const = 0;
  virtual void MapEnd(MapIterator* map_iter) const = 0;
  virtual void MergeFrom(const MapFieldBase& other) = 0;
  virtual void Swap(MapFieldBase* other);
  virtual void UnsafeShallowSwap(MapFieldBase* other);
  // Sync Map with repeated field and returns the size of map.
  virtual int size() const = 0;
  virtual void Clear() = 0;

  // Returns the number of bytes used by the repeated field, excluding
  // sizeof(*this)
  size_t SpaceUsedExcludingSelfLong() const;

  int SpaceUsedExcludingSelf() const {
    return internal::ToIntSize(SpaceUsedExcludingSelfLong());
  }

 protected:
  // Gets the size of space used by map field.
  virtual size_t SpaceUsedExcludingSelfNoLock() const;

  // Synchronizes the content in Map to RepeatedPtrField if there is any change
  // to Map after last synchronization.
  void SyncRepeatedFieldWithMap() const;
  virtual void SyncRepeatedFieldWithMapNoLock() const = 0;

  // Synchronizes the content in RepeatedPtrField to Map if there is any change
  // to RepeatedPtrField after last synchronization.
  void SyncMapWithRepeatedField() const;
  virtual void SyncMapWithRepeatedFieldNoLock() const = 0;

  // Tells MapFieldBase that there is new change to Map.
  void SetMapDirty();

  // Tells MapFieldBase that there is new change to RepeatedPtrField.
  void SetRepeatedDirty();

  // Provides derived class the access to repeated field.
  void* MutableRepeatedPtrField() const;

  void InternalSwap(MapFieldBase* other);

  // Support thread sanitizer (tsan) by making const / mutable races
  // more apparent.  If one thread calls MutableAccess() while another
  // thread calls either ConstAccess() or MutableAccess(), on the same
  // MapFieldBase-derived object, and there is no synchronization going
  // on between them, tsan will alert.
#if defined(__SANITIZE_THREAD__) || defined(THREAD_SANITIZER)
  void ConstAccess() const { ABSL_CHECK_EQ(seq1_, seq2_); }
  void MutableAccess() {
    if (seq1_ & 1) {
      seq2_ = ++seq1_;
    } else {
      seq1_ = ++seq2_;
    }
  }
  unsigned int seq1_ = 0, seq2_ = 0;
#else
  void ConstAccess() const {}
  void MutableAccess() {}
#endif
  enum State {
    STATE_MODIFIED_MAP = 0,       // map has newly added data that has not been
                                  // synchronized to repeated field
    STATE_MODIFIED_REPEATED = 1,  // repeated field has newly added data that
                                  // has not been synchronized to map
    CLEAN = 2,                    // data in map and repeated field are same
  };

  struct ReflectionPayload {
    explicit ReflectionPayload(Arena* arena) : repeated_field(arena) {}
    RepeatedPtrField<Message> repeated_field;

    absl::Mutex mutex;  // The thread to synchronize map and repeated
                        // field needs to get lock first;
    std::atomic<State> state{STATE_MODIFIED_MAP};
  };

  Arena* arena() const {
    auto p = payload_.load(std::memory_order_acquire);
    if (IsPayload(p)) return ToPayload(p)->repeated_field.GetArena();
    return ToArena(p);
  }

  // Returns the reflection payload. Returns null if it does not exist yet.
  ReflectionPayload* maybe_payload() const {
    auto p = payload_.load(std::memory_order_acquire);
    return IsPayload(p) ? ToPayload(p) : nullptr;
  }
  // Returns the reflection payload, and constructs one if does not exist yet.
  ReflectionPayload& payload() const {
    auto* p = maybe_payload();
    return p != nullptr ? *p : PayloadSlow();
  }
  ReflectionPayload& PayloadSlow() const;

  State state() const {
    auto* p = maybe_payload();
    return p != nullptr ? p->state.load(std::memory_order_acquire)
                        // The default
                        : STATE_MODIFIED_MAP;
  }

 private:
  friend class ContendedMapCleanTest;
  friend class GeneratedMessageReflection;
  friend class MapFieldAccessor;
  friend class google::protobuf::Reflection;
  friend class google::protobuf::DynamicMessage;

  // Virtual helper methods for MapIterator. MapIterator doesn't have the
  // type helper for key and value. Call these help methods to deal with
  // different types. Real helper methods are implemented in
  // TypeDefinedMapFieldBase.
  friend class google::protobuf::MapIterator;

  // Copy the map<...>::iterator from other_iterator to
  // this_iterator.
  virtual void CopyIterator(MapIterator* this_iterator,
                            const MapIterator& other_iterator) const = 0;

  // IncreaseIterator() is called by operator++() of MapIterator only.
  // It implements the ++ operator of MapIterator.
  virtual void IncreaseIterator(MapIterator* map_iter) const = 0;

  enum class TaggedPtr : uintptr_t {};
  static constexpr uintptr_t kHasPayloadBit = 1;

  static ReflectionPayload* ToPayload(TaggedPtr p) {
    ABSL_DCHECK(IsPayload(p));
    auto* res = reinterpret_cast<ReflectionPayload*>(static_cast<uintptr_t>(p) -
                                                     kHasPayloadBit);
    PROTOBUF_ASSUME(res != nullptr);
    return res;
  }
  static Arena* ToArena(TaggedPtr p) {
    ABSL_DCHECK(!IsPayload(p));
    return reinterpret_cast<Arena*>(p);
  }
  static TaggedPtr ToTaggedPtr(ReflectionPayload* p) {
    return static_cast<TaggedPtr>(reinterpret_cast<uintptr_t>(p) +
                                  kHasPayloadBit);
  }
  static TaggedPtr ToTaggedPtr(Arena* p) {
    return static_cast<TaggedPtr>(reinterpret_cast<uintptr_t>(p));
  }
  static bool IsPayload(TaggedPtr p) {
    return static_cast<uintptr_t>(p) & kHasPayloadBit;
  }

  mutable std::atomic<TaggedPtr> payload_{};
};

// This class provides common Map Reflection implementations for generated
// message and dynamic message.
template <typename Key, typename T>
class TypeDefinedMapFieldBase : public MapFieldBase {
 public:
  constexpr TypeDefinedMapFieldBase() : map_() {}
  TypeDefinedMapFieldBase(const TypeDefinedMapFieldBase&) = delete;
  TypeDefinedMapFieldBase& operator=(const TypeDefinedMapFieldBase&) = delete;

  explicit TypeDefinedMapFieldBase(Arena* arena)
      : MapFieldBase(arena), map_(arena) {}
  TypeDefinedMapFieldBase(ArenaInitialized, Arena* arena)
      : TypeDefinedMapFieldBase(arena) {}

 protected:
  ~TypeDefinedMapFieldBase() { map_.~Map(); }

  // Not all overrides are marked `final` here because DynamicMapField overrides
  // them. DynamicMapField does extra memory management for the elements and
  // needs to override the functions that create or destroy elements.

 public:
  void MapBegin(MapIterator* map_iter) const final;
  void MapEnd(MapIterator* map_iter) const final;
  bool EqualIterator(const MapIterator& a, const MapIterator& b) const final;

  const Map<Key, T>& GetMap() const {
    SyncMapWithRepeatedField();
    return map_;
  }

  Map<Key, T>* MutableMap() override final {
    SyncMapWithRepeatedField();
    SetMapDirty();
    return &map_;
  }

  void Clear() override {
    if (auto* p = maybe_payload()) {
      p->repeated_field.Clear();
    }

    MutableMap()->clear();
    // Data in map and repeated field are both empty, but we can't set status
    // CLEAN. Because clear is a generated API, we cannot invalidate previous
    // reference to map.
    SetMapDirty();
  }

  int size() const final { return GetMap().size(); }

  void InternalSwap(TypeDefinedMapFieldBase* other);

  void Swap(MapFieldBase* other) final;

  void UnsafeShallowSwap(MapFieldBase* other) override;
  size_t SpaceUsedExcludingSelfNoLock() const override;
  void MergeFrom(const MapFieldBase& other) override;

 protected:
  using Iter = typename Map<Key, T>::const_iterator;
  static const Iter& InternalGetIterator(const MapIterator* map_iter);
  static Iter& InternalGetIterator(MapIterator* map_iter);

  // map_ is inside an anonymous union so we can explicitly control its
  // destruction
  union {
    Map<Key, T> map_;
  };

 private:
  void CopyIterator(MapIterator* this_iteratorm,
                    const MapIterator& that_iterator) const final;
  void IncreaseIterator(MapIterator* map_iter) const final;
  void SetMapIteratorValue(MapIterator* map_iter) const;
  bool ContainsMapKey(const MapKey& map_key) const final;
  bool LookupMapValue(const MapKey& map_key, MapValueConstRef* val) const final;
  bool LookupMapValue(const MapKey&, MapValueRef*) const = delete;
  bool DeleteMapValue(const MapKey& map_key) final;
  bool InsertOrLookupMapValue(const MapKey& map_key, MapValueRef* val) override;
};

// This class provides access to map field using generated api. It is used for
// internal generated message implementation only. Users should never use this
// directly.
template <typename Derived, typename Key, typename T,
          WireFormatLite::FieldType kKeyFieldType_,
          WireFormatLite::FieldType kValueFieldType_>
class MapField final : public TypeDefinedMapFieldBase<Key, T> {
  // Provide utilities to parse/serialize key/value.  Provide utilities to
  // manipulate internal stored type.
  typedef MapTypeHandler<kKeyFieldType_, Key> KeyTypeHandler;
  typedef MapTypeHandler<kValueFieldType_, T> ValueTypeHandler;

  // Define message type for internal repeated field.
  typedef Derived EntryType;

  // Define abbreviation for parent MapFieldLite
  typedef MapFieldLite<Derived, Key, T, kKeyFieldType_, kValueFieldType_>
      MapFieldLiteType;

  // Enum needs to be handled differently from other types because it has
  // different exposed type in Map's api and repeated field's api. For
  // details see the comment in the implementation of
  // SyncMapWithRepeatedFieldNoLock.
  static constexpr bool kIsValueEnum = ValueTypeHandler::kIsEnum;
  typedef typename MapIf<kIsValueEnum, T, const T&>::type CastValueType;

 public:
  typedef Map<Key, T> MapType;
  static constexpr WireFormatLite::FieldType kKeyFieldType = kKeyFieldType_;
  static constexpr WireFormatLite::FieldType kValueFieldType = kValueFieldType_;

  constexpr MapField() {}
  MapField(const MapField&) = delete;
  MapField& operator=(const MapField&) = delete;
  ~MapField() {}

  explicit MapField(Arena* arena) : TypeDefinedMapFieldBase<Key, T>(arena) {}
  MapField(ArenaInitialized, Arena* arena) : MapField(arena) {}

  // Used in the implementation of parsing. Caller should take the ownership iff
  // arena_ is nullptr.
  EntryType* NewEntry() const {
    return Arena::CreateMessage<EntryType>(this->arena());
  }

  const char* _InternalParse(const char* ptr, ParseContext* ctx) {
    typename Derived::template Parser<MapField, Map<Key, T>> parser(this);
    return parser._InternalParse(ptr, ctx);
  }
  template <typename UnknownType>
  const char* ParseWithEnumValidation(const char* ptr, ParseContext* ctx,
                                      bool (*is_valid)(int), uint32_t field_num,
                                      InternalMetadata* metadata) {
    typename Derived::template Parser<MapField, Map<Key, T>> parser(this);
    return parser.template ParseWithEnumValidation<UnknownType>(
        ptr, ctx, is_valid, field_num, metadata);
  }

 private:
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;

  // Implements MapFieldBase
  void SyncRepeatedFieldWithMapNoLock() const final;
  void SyncMapWithRepeatedFieldNoLock() const final;

  friend class google::protobuf::Arena;
  friend class MapFieldStateTest;  // For testing, it needs raw access to impl_
};

template <typename Key, typename T>
bool AllAreInitialized(const TypeDefinedMapFieldBase<Key, T>& field) {
  for (const auto& p : field.GetMap()) {
    if (!p.second.IsInitialized()) return false;
  }
  return true;
}

template <typename T, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType>
struct MapEntryToMapField<
    MapEntry<T, Key, Value, kKeyFieldType, kValueFieldType>> {
  typedef MapField<T, Key, Value, kKeyFieldType, kValueFieldType> MapFieldType;
};

class PROTOBUF_EXPORT DynamicMapField final
    : public TypeDefinedMapFieldBase<MapKey, MapValueRef> {
 public:
  explicit DynamicMapField(const Message* default_entry);
  DynamicMapField(const Message* default_entry, Arena* arena);
  DynamicMapField(const DynamicMapField&) = delete;
  DynamicMapField& operator=(const DynamicMapField&) = delete;
  virtual ~DynamicMapField();

  // Implement MapFieldBase
  bool InsertOrLookupMapValue(const MapKey& map_key, MapValueRef* val) final;
  void MergeFrom(const MapFieldBase& other) final;
  void UnsafeShallowSwap(MapFieldBase* other) final { Swap(other); }

  void Clear() final;

 private:
  const Message* default_entry_;

  void AllocateMapValue(MapValueRef* map_val);

  // Implements MapFieldBase
  void SyncRepeatedFieldWithMapNoLock() const final;
  void SyncMapWithRepeatedFieldNoLock() const final;
  size_t SpaceUsedExcludingSelfNoLock() const final;
};

}  // namespace internal

// MapValueConstRef points to a map value. Users can NOT modify
// the map value.
class PROTOBUF_EXPORT MapValueConstRef {
 public:
  MapValueConstRef() : data_(nullptr), type_() {}

  int64_t GetInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT64,
               "MapValueConstRef::GetInt64Value");
    return *reinterpret_cast<int64_t*>(data_);
  }
  uint64_t GetUInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT64,
               "MapValueConstRef::GetUInt64Value");
    return *reinterpret_cast<uint64_t*>(data_);
  }
  int32_t GetInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT32,
               "MapValueConstRef::GetInt32Value");
    return *reinterpret_cast<int32_t*>(data_);
  }
  uint32_t GetUInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT32,
               "MapValueConstRef::GetUInt32Value");
    return *reinterpret_cast<uint32_t*>(data_);
  }
  bool GetBoolValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_BOOL, "MapValueConstRef::GetBoolValue");
    return *reinterpret_cast<bool*>(data_);
  }
  int GetEnumValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_ENUM, "MapValueConstRef::GetEnumValue");
    return *reinterpret_cast<int*>(data_);
  }
  const std::string& GetStringValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_STRING,
               "MapValueConstRef::GetStringValue");
    return *reinterpret_cast<std::string*>(data_);
  }
  float GetFloatValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_FLOAT,
               "MapValueConstRef::GetFloatValue");
    return *reinterpret_cast<float*>(data_);
  }
  double GetDoubleValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_DOUBLE,
               "MapValueConstRef::GetDoubleValue");
    return *reinterpret_cast<double*>(data_);
  }

  const Message& GetMessageValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_MESSAGE,
               "MapValueConstRef::GetMessageValue");
    return *reinterpret_cast<Message*>(data_);
  }

 protected:
  // data_ point to a map value. MapValueConstRef does not
  // own this value.
  void* data_;
  // type_ is 0 or a valid FieldDescriptor::CppType.
  // Use "CppType()" to indicate zero.
  FieldDescriptor::CppType type_;

  FieldDescriptor::CppType type() const {
    if (type_ == FieldDescriptor::CppType() || data_ == nullptr) {
      ABSL_LOG(FATAL)
          << "Protocol Buffer map usage error:\n"
          << "MapValueConstRef::type MapValueConstRef is not initialized.";
    }
    return type_;
  }

 private:
  template <typename Derived, typename K, typename V,
            internal::WireFormatLite::FieldType key_wire_type,
            internal::WireFormatLite::FieldType value_wire_type>
  friend class internal::MapField;
  template <typename K, typename V>
  friend class internal::TypeDefinedMapFieldBase;
  friend class google::protobuf::MapIterator;
  friend class Reflection;
  friend class internal::DynamicMapField;

  void SetValueOrCopy(const void* val) { SetValue(val); }
  void SetValueOrCopy(const MapValueConstRef* val) { CopyFrom(*val); }

  void SetType(FieldDescriptor::CppType type) { type_ = type; }
  void SetValue(const void* val) { data_ = const_cast<void*>(val); }
  void CopyFrom(const MapValueConstRef& other) {
    type_ = other.type_;
    data_ = other.data_;
  }
};

// MapValueRef points to a map value. Users are able to modify
// the map value.
class PROTOBUF_EXPORT MapValueRef final : public MapValueConstRef {
 public:
  MapValueRef() {}

  void SetInt64Value(int64_t value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT64, "MapValueRef::SetInt64Value");
    *reinterpret_cast<int64_t*>(data_) = value;
  }
  void SetUInt64Value(uint64_t value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT64, "MapValueRef::SetUInt64Value");
    *reinterpret_cast<uint64_t*>(data_) = value;
  }
  void SetInt32Value(int32_t value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT32, "MapValueRef::SetInt32Value");
    *reinterpret_cast<int32_t*>(data_) = value;
  }
  void SetUInt32Value(uint32_t value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT32, "MapValueRef::SetUInt32Value");
    *reinterpret_cast<uint32_t*>(data_) = value;
  }
  void SetBoolValue(bool value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_BOOL, "MapValueRef::SetBoolValue");
    *reinterpret_cast<bool*>(data_) = value;
  }
  // TODO(jieluo) - Checks that enum is member.
  void SetEnumValue(int value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_ENUM, "MapValueRef::SetEnumValue");
    *reinterpret_cast<int*>(data_) = value;
  }
  void SetStringValue(const std::string& value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_STRING, "MapValueRef::SetStringValue");
    *reinterpret_cast<std::string*>(data_) = value;
  }
  void SetFloatValue(float value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_FLOAT, "MapValueRef::SetFloatValue");
    *reinterpret_cast<float*>(data_) = value;
  }
  void SetDoubleValue(double value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_DOUBLE, "MapValueRef::SetDoubleValue");
    *reinterpret_cast<double*>(data_) = value;
  }

  Message* MutableMessageValue() {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_MESSAGE,
               "MapValueRef::MutableMessageValue");
    return reinterpret_cast<Message*>(data_);
  }

 private:
  friend class internal::DynamicMapField;

  // Only used in DynamicMapField
  void DeleteData() {
    switch (type_) {
#define HANDLE_TYPE(CPPTYPE, TYPE)           \
  case FieldDescriptor::CPPTYPE_##CPPTYPE: { \
    delete reinterpret_cast<TYPE*>(data_);   \
    break;                                   \
  }
      HANDLE_TYPE(INT32, int32_t);
      HANDLE_TYPE(INT64, int64_t);
      HANDLE_TYPE(UINT32, uint32_t);
      HANDLE_TYPE(UINT64, uint64_t);
      HANDLE_TYPE(DOUBLE, double);
      HANDLE_TYPE(FLOAT, float);
      HANDLE_TYPE(BOOL, bool);
      HANDLE_TYPE(STRING, std::string);
      HANDLE_TYPE(ENUM, int32_t);
      HANDLE_TYPE(MESSAGE, Message);
#undef HANDLE_TYPE
    }
  }
};

#undef TYPE_CHECK

class PROTOBUF_EXPORT MapIterator {
 public:
  MapIterator(Message* message, const FieldDescriptor* field) {
    const Reflection* reflection = message->GetReflection();
    map_ = reflection->MutableMapData(message, field);
    key_.SetType(field->message_type()->map_key()->cpp_type());
    value_.SetType(field->message_type()->map_value()->cpp_type());
  }
  MapIterator(const MapIterator& other) { *this = other; }
  MapIterator& operator=(const MapIterator& other) {
    map_ = other.map_;
    map_->CopyIterator(this, other);
    return *this;
  }
  friend bool operator==(const MapIterator& a, const MapIterator& b) {
    return a.map_->EqualIterator(a, b);
  }
  friend bool operator!=(const MapIterator& a, const MapIterator& b) {
    return !a.map_->EqualIterator(a, b);
  }
  MapIterator& operator++() {
    map_->IncreaseIterator(this);
    return *this;
  }
  MapIterator operator++(int) {
    // iter_ is copied from Map<...>::iterator, no need to
    // copy from its self again. Use the same implementation
    // with operator++()
    map_->IncreaseIterator(this);
    return *this;
  }
  const MapKey& GetKey() { return key_; }
  const MapValueRef& GetValueRef() { return value_; }
  MapValueRef* MutableValueRef() {
    map_->SetMapDirty();
    return &value_;
  }

 private:
  template <typename Key, typename T>
  friend class internal::TypeDefinedMapFieldBase;
  friend class internal::DynamicMapField;
  template <typename Derived, typename Key, typename T,
            internal::WireFormatLite::FieldType kKeyFieldType,
            internal::WireFormatLite::FieldType kValueFieldType>
  friend class internal::MapField;

  // This field provides the storage for Map<...>::const_iterator. We use
  // reinterpret_cast to get the right type. The real iterator is trivially
  // destructible/copyable, so no need to manage that.
  alignas(internal::MapIteratorPayload) char map_iter_buffer_[sizeof(
      internal::MapIteratorPayload)]{};
  // Point to a MapField to call helper methods implemented in MapField.
  // MapIterator does not own this object.
  internal::MapFieldBase* map_;
  MapKey key_;
  MapValueRef value_;
};

namespace internal {
template <>
struct is_internal_map_value_type<class MapValueConstRef> : std::true_type {};
template <>
struct is_internal_map_value_type<class MapValueRef> : std::true_type {};
}  // namespace internal

}  // namespace protobuf
}  // namespace google

#ifdef _MSC_VER
#pragma warning(pop)  // restore warning C4265
#endif                // _MSC_VER

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_MAP_FIELD_H__
