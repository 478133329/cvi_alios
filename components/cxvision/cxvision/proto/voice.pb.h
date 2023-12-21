/*
 * Copyright (C) 2019-2022 Alibaba Group Holding Limited
 */

// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: voice.proto

#ifndef GOOGLE_PROTOBUF_INCLUDED_voice_2eproto
#define GOOGLE_PROTOBUF_INCLUDED_voice_2eproto

#include <limits>
#include <string>

#include <google/protobuf/port_def.inc>
#if PROTOBUF_VERSION < 3017000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers. Please update
#error your headers.
#endif
#if 3017003 < PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers. Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/port_undef.inc>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_table_driven.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/generated_enum_util.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>
#define PROTOBUF_INTERNAL_EXPORT_voice_2eproto
PROTOBUF_NAMESPACE_OPEN
namespace internal {
class AnyMetadata;
}  // namespace internal
PROTOBUF_NAMESPACE_CLOSE

// Internal implementation detail -- do not use these members.
struct TableStruct_voice_2eproto {
  static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTableField entries[]
    PROTOBUF_SECTION_VARIABLE(protodesc_cold);
  static const ::PROTOBUF_NAMESPACE_ID::internal::AuxiliaryParseTableField aux[]
    PROTOBUF_SECTION_VARIABLE(protodesc_cold);
  static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTable schema[2]
    PROTOBUF_SECTION_VARIABLE(protodesc_cold);
  static const ::PROTOBUF_NAMESPACE_ID::internal::FieldMetadata field_metadata[];
  static const ::PROTOBUF_NAMESPACE_ID::internal::SerializationTable serialization_table[];
  static const ::PROTOBUF_NAMESPACE_ID::uint32 offsets[];
};
namespace thead {
namespace voice {
namespace proto {
class RecordMsg;
struct RecordMsgDefaultTypeInternal;
extern RecordMsgDefaultTypeInternal _RecordMsg_default_instance_;
class SessionMsg;
struct SessionMsgDefaultTypeInternal;
extern SessionMsgDefaultTypeInternal _SessionMsg_default_instance_;
}  // namespace proto
}  // namespace voice
}  // namespace thead
PROTOBUF_NAMESPACE_OPEN
template<> ::thead::voice::proto::RecordMsg* Arena::CreateMaybeMessage<::thead::voice::proto::RecordMsg>(Arena*);
template<> ::thead::voice::proto::SessionMsg* Arena::CreateMaybeMessage<::thead::voice::proto::SessionMsg>(Arena*);
PROTOBUF_NAMESPACE_CLOSE
namespace thead {
namespace voice {
namespace proto {

enum SessionCmd : int {
  BEGIN = 0,
  END = 1,
  TIMEOUT = 2,
  SessionCmd_INT_MIN_SENTINEL_DO_NOT_USE_ = std::numeric_limits<::PROTOBUF_NAMESPACE_ID::int32>::min(),
  SessionCmd_INT_MAX_SENTINEL_DO_NOT_USE_ = std::numeric_limits<::PROTOBUF_NAMESPACE_ID::int32>::max()
};
bool SessionCmd_IsValid(int value);
constexpr SessionCmd SessionCmd_MIN = BEGIN;
constexpr SessionCmd SessionCmd_MAX = TIMEOUT;
constexpr int SessionCmd_ARRAYSIZE = SessionCmd_MAX + 1;

const std::string& SessionCmd_Name(SessionCmd value);
template<typename T>
inline const std::string& SessionCmd_Name(T enum_t_value) {
  static_assert(::std::is_same<T, SessionCmd>::value ||
    ::std::is_integral<T>::value,
    "Incorrect type passed to function SessionCmd_Name.");
  return SessionCmd_Name(static_cast<SessionCmd>(enum_t_value));
}
bool SessionCmd_Parse(
    ::PROTOBUF_NAMESPACE_ID::ConstStringParam name, SessionCmd* value);
enum RecordCmd : int {
  START = 0,
  STOP = 1,
  RecordCmd_INT_MIN_SENTINEL_DO_NOT_USE_ = std::numeric_limits<::PROTOBUF_NAMESPACE_ID::int32>::min(),
  RecordCmd_INT_MAX_SENTINEL_DO_NOT_USE_ = std::numeric_limits<::PROTOBUF_NAMESPACE_ID::int32>::max()
};
bool RecordCmd_IsValid(int value);
constexpr RecordCmd RecordCmd_MIN = START;
constexpr RecordCmd RecordCmd_MAX = STOP;
constexpr int RecordCmd_ARRAYSIZE = RecordCmd_MAX + 1;

const std::string& RecordCmd_Name(RecordCmd value);
template<typename T>
inline const std::string& RecordCmd_Name(T enum_t_value) {
  static_assert(::std::is_same<T, RecordCmd>::value ||
    ::std::is_integral<T>::value,
    "Incorrect type passed to function RecordCmd_Name.");
  return RecordCmd_Name(static_cast<RecordCmd>(enum_t_value));
}
bool RecordCmd_Parse(
    ::PROTOBUF_NAMESPACE_ID::ConstStringParam name, RecordCmd* value);
// ===================================================================

class SessionMsg final :
    public ::PROTOBUF_NAMESPACE_ID::MessageLite /* @@protoc_insertion_point(class_definition:thead.voice.proto.SessionMsg) */ {
 public:
  inline SessionMsg() : SessionMsg(nullptr) {}
  ~SessionMsg() override;
  explicit constexpr SessionMsg(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized);

  SessionMsg(const SessionMsg& from);
  SessionMsg(SessionMsg&& from) noexcept
    : SessionMsg() {
    *this = ::std::move(from);
  }

  inline SessionMsg& operator=(const SessionMsg& from) {
    CopyFrom(from);
    return *this;
  }
  inline SessionMsg& operator=(SessionMsg&& from) noexcept {
    if (this == &from) return *this;
    if (GetOwningArena() == from.GetOwningArena()) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  static const SessionMsg& default_instance() {
    return *internal_default_instance();
  }
  static inline const SessionMsg* internal_default_instance() {
    return reinterpret_cast<const SessionMsg*>(
               &_SessionMsg_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    0;

  friend void swap(SessionMsg& a, SessionMsg& b) {
    a.Swap(&b);
  }
  inline void Swap(SessionMsg* other) {
    if (other == this) return;
    if (GetOwningArena() == other->GetOwningArena()) {
      InternalSwap(other);
    } else {
      ::PROTOBUF_NAMESPACE_ID::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(SessionMsg* other) {
    if (other == this) return;
    GOOGLE_DCHECK(GetOwningArena() == other->GetOwningArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  inline SessionMsg* New() const final {
    return new SessionMsg();
  }

  SessionMsg* New(::PROTOBUF_NAMESPACE_ID::Arena* arena) const final {
    return CreateMaybeMessage<SessionMsg>(arena);
  }
  void CheckTypeAndMergeFrom(const ::PROTOBUF_NAMESPACE_ID::MessageLite& from)  final;
  void CopyFrom(const SessionMsg& from);
  void MergeFrom(const SessionMsg& from);
  PROTOBUF_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) final;
  ::PROTOBUF_NAMESPACE_ID::uint8* _InternalSerialize(
      ::PROTOBUF_NAMESPACE_ID::uint8* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const final;
  void DiscardUnknownFields();
  int GetCachedSize() const final { return _cached_size_.Get(); }

  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  void InternalSwap(SessionMsg* other);
  friend class ::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata;
  static ::PROTOBUF_NAMESPACE_ID::StringPiece FullMessageName() {
    return "thead.voice.proto.SessionMsg";
  }
  protected:
  explicit SessionMsg(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                       bool is_message_owned = false);
  private:
  static void ArenaDtor(void* object);
  inline void RegisterArenaDtor(::PROTOBUF_NAMESPACE_ID::Arena* arena);
  public:

  std::string GetTypeName() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kKwsWordFieldNumber = 3,
    kCmdIdFieldNumber = 1,
    kKwsIdFieldNumber = 2,
    kKwsScoreFieldNumber = 4,
  };
  // string kws_word = 3;
  void clear_kws_word();
  const std::string& kws_word() const;
  template <typename ArgT0 = const std::string&, typename... ArgT>
  void set_kws_word(ArgT0&& arg0, ArgT... args);
  std::string* mutable_kws_word();
  PROTOBUF_MUST_USE_RESULT std::string* release_kws_word();
  void set_allocated_kws_word(std::string* kws_word);
  private:
  const std::string& _internal_kws_word() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_kws_word(const std::string& value);
  std::string* _internal_mutable_kws_word();
  public:

  // .thead.voice.proto.SessionCmd cmd_id = 1;
  void clear_cmd_id();
  ::thead::voice::proto::SessionCmd cmd_id() const;
  void set_cmd_id(::thead::voice::proto::SessionCmd value);
  private:
  ::thead::voice::proto::SessionCmd _internal_cmd_id() const;
  void _internal_set_cmd_id(::thead::voice::proto::SessionCmd value);
  public:

  // int32 kws_id = 2;
  void clear_kws_id();
  ::PROTOBUF_NAMESPACE_ID::int32 kws_id() const;
  void set_kws_id(::PROTOBUF_NAMESPACE_ID::int32 value);
  private:
  ::PROTOBUF_NAMESPACE_ID::int32 _internal_kws_id() const;
  void _internal_set_kws_id(::PROTOBUF_NAMESPACE_ID::int32 value);
  public:

  // int32 kws_score = 4;
  void clear_kws_score();
  ::PROTOBUF_NAMESPACE_ID::int32 kws_score() const;
  void set_kws_score(::PROTOBUF_NAMESPACE_ID::int32 value);
  private:
  ::PROTOBUF_NAMESPACE_ID::int32 _internal_kws_score() const;
  void _internal_set_kws_score(::PROTOBUF_NAMESPACE_ID::int32 value);
  public:

  // @@protoc_insertion_point(class_scope:thead.voice.proto.SessionMsg)
 private:
  class _Internal;

  template <typename T> friend class ::PROTOBUF_NAMESPACE_ID::Arena::InternalHelper;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr kws_word_;
  int cmd_id_;
  ::PROTOBUF_NAMESPACE_ID::int32 kws_id_;
  ::PROTOBUF_NAMESPACE_ID::int32 kws_score_;
  mutable ::PROTOBUF_NAMESPACE_ID::internal::CachedSize _cached_size_;
  friend struct ::TableStruct_voice_2eproto;
};
// -------------------------------------------------------------------

class RecordMsg final :
    public ::PROTOBUF_NAMESPACE_ID::MessageLite /* @@protoc_insertion_point(class_definition:thead.voice.proto.RecordMsg) */ {
 public:
  inline RecordMsg() : RecordMsg(nullptr) {}
  ~RecordMsg() override;
  explicit constexpr RecordMsg(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized);

  RecordMsg(const RecordMsg& from);
  RecordMsg(RecordMsg&& from) noexcept
    : RecordMsg() {
    *this = ::std::move(from);
  }

  inline RecordMsg& operator=(const RecordMsg& from) {
    CopyFrom(from);
    return *this;
  }
  inline RecordMsg& operator=(RecordMsg&& from) noexcept {
    if (this == &from) return *this;
    if (GetOwningArena() == from.GetOwningArena()) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  static const RecordMsg& default_instance() {
    return *internal_default_instance();
  }
  static inline const RecordMsg* internal_default_instance() {
    return reinterpret_cast<const RecordMsg*>(
               &_RecordMsg_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    1;

  friend void swap(RecordMsg& a, RecordMsg& b) {
    a.Swap(&b);
  }
  inline void Swap(RecordMsg* other) {
    if (other == this) return;
    if (GetOwningArena() == other->GetOwningArena()) {
      InternalSwap(other);
    } else {
      ::PROTOBUF_NAMESPACE_ID::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(RecordMsg* other) {
    if (other == this) return;
    GOOGLE_DCHECK(GetOwningArena() == other->GetOwningArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  inline RecordMsg* New() const final {
    return new RecordMsg();
  }

  RecordMsg* New(::PROTOBUF_NAMESPACE_ID::Arena* arena) const final {
    return CreateMaybeMessage<RecordMsg>(arena);
  }
  void CheckTypeAndMergeFrom(const ::PROTOBUF_NAMESPACE_ID::MessageLite& from)  final;
  void CopyFrom(const RecordMsg& from);
  void MergeFrom(const RecordMsg& from);
  PROTOBUF_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) final;
  ::PROTOBUF_NAMESPACE_ID::uint8* _InternalSerialize(
      ::PROTOBUF_NAMESPACE_ID::uint8* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const final;
  void DiscardUnknownFields();
  int GetCachedSize() const final { return _cached_size_.Get(); }

  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  void InternalSwap(RecordMsg* other);
  friend class ::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata;
  static ::PROTOBUF_NAMESPACE_ID::StringPiece FullMessageName() {
    return "thead.voice.proto.RecordMsg";
  }
  protected:
  explicit RecordMsg(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                       bool is_message_owned = false);
  private:
  static void ArenaDtor(void* object);
  inline void RegisterArenaDtor(::PROTOBUF_NAMESPACE_ID::Arena* arena);
  public:

  std::string GetTypeName() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kCmdFieldNumber = 1,
  };
  // .thead.voice.proto.RecordCmd cmd = 1;
  void clear_cmd();
  ::thead::voice::proto::RecordCmd cmd() const;
  void set_cmd(::thead::voice::proto::RecordCmd value);
  private:
  ::thead::voice::proto::RecordCmd _internal_cmd() const;
  void _internal_set_cmd(::thead::voice::proto::RecordCmd value);
  public:

  // @@protoc_insertion_point(class_scope:thead.voice.proto.RecordMsg)
 private:
  class _Internal;

  template <typename T> friend class ::PROTOBUF_NAMESPACE_ID::Arena::InternalHelper;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  int cmd_;
  mutable ::PROTOBUF_NAMESPACE_ID::internal::CachedSize _cached_size_;
  friend struct ::TableStruct_voice_2eproto;
};
// ===================================================================


// ===================================================================

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// SessionMsg

// .thead.voice.proto.SessionCmd cmd_id = 1;
inline void SessionMsg::clear_cmd_id() {
  cmd_id_ = 0;
}
inline ::thead::voice::proto::SessionCmd SessionMsg::_internal_cmd_id() const {
  return static_cast< ::thead::voice::proto::SessionCmd >(cmd_id_);
}
inline ::thead::voice::proto::SessionCmd SessionMsg::cmd_id() const {
  // @@protoc_insertion_point(field_get:thead.voice.proto.SessionMsg.cmd_id)
  return _internal_cmd_id();
}
inline void SessionMsg::_internal_set_cmd_id(::thead::voice::proto::SessionCmd value) {
  
  cmd_id_ = value;
}
inline void SessionMsg::set_cmd_id(::thead::voice::proto::SessionCmd value) {
  _internal_set_cmd_id(value);
  // @@protoc_insertion_point(field_set:thead.voice.proto.SessionMsg.cmd_id)
}

// int32 kws_id = 2;
inline void SessionMsg::clear_kws_id() {
  kws_id_ = 0;
}
inline ::PROTOBUF_NAMESPACE_ID::int32 SessionMsg::_internal_kws_id() const {
  return kws_id_;
}
inline ::PROTOBUF_NAMESPACE_ID::int32 SessionMsg::kws_id() const {
  // @@protoc_insertion_point(field_get:thead.voice.proto.SessionMsg.kws_id)
  return _internal_kws_id();
}
inline void SessionMsg::_internal_set_kws_id(::PROTOBUF_NAMESPACE_ID::int32 value) {
  
  kws_id_ = value;
}
inline void SessionMsg::set_kws_id(::PROTOBUF_NAMESPACE_ID::int32 value) {
  _internal_set_kws_id(value);
  // @@protoc_insertion_point(field_set:thead.voice.proto.SessionMsg.kws_id)
}

// string kws_word = 3;
inline void SessionMsg::clear_kws_word() {
  kws_word_.ClearToEmpty();
}
inline const std::string& SessionMsg::kws_word() const {
  // @@protoc_insertion_point(field_get:thead.voice.proto.SessionMsg.kws_word)
  return _internal_kws_word();
}
template <typename ArgT0, typename... ArgT>
inline PROTOBUF_ALWAYS_INLINE
void SessionMsg::set_kws_word(ArgT0&& arg0, ArgT... args) {
 
 kws_word_.Set(::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::EmptyDefault{}, static_cast<ArgT0 &&>(arg0), args..., GetArenaForAllocation());
  // @@protoc_insertion_point(field_set:thead.voice.proto.SessionMsg.kws_word)
}
inline std::string* SessionMsg::mutable_kws_word() {
  std::string* _s = _internal_mutable_kws_word();
  // @@protoc_insertion_point(field_mutable:thead.voice.proto.SessionMsg.kws_word)
  return _s;
}
inline const std::string& SessionMsg::_internal_kws_word() const {
  return kws_word_.Get();
}
inline void SessionMsg::_internal_set_kws_word(const std::string& value) {
  
  kws_word_.Set(::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::EmptyDefault{}, value, GetArenaForAllocation());
}
inline std::string* SessionMsg::_internal_mutable_kws_word() {
  
  return kws_word_.Mutable(::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::EmptyDefault{}, GetArenaForAllocation());
}
inline std::string* SessionMsg::release_kws_word() {
  // @@protoc_insertion_point(field_release:thead.voice.proto.SessionMsg.kws_word)
  return kws_word_.Release(&::PROTOBUF_NAMESPACE_ID::internal::GetEmptyStringAlreadyInited(), GetArenaForAllocation());
}
inline void SessionMsg::set_allocated_kws_word(std::string* kws_word) {
  if (kws_word != nullptr) {
    
  } else {
    
  }
  kws_word_.SetAllocated(&::PROTOBUF_NAMESPACE_ID::internal::GetEmptyStringAlreadyInited(), kws_word,
      GetArenaForAllocation());
  // @@protoc_insertion_point(field_set_allocated:thead.voice.proto.SessionMsg.kws_word)
}

// int32 kws_score = 4;
inline void SessionMsg::clear_kws_score() {
  kws_score_ = 0;
}
inline ::PROTOBUF_NAMESPACE_ID::int32 SessionMsg::_internal_kws_score() const {
  return kws_score_;
}
inline ::PROTOBUF_NAMESPACE_ID::int32 SessionMsg::kws_score() const {
  // @@protoc_insertion_point(field_get:thead.voice.proto.SessionMsg.kws_score)
  return _internal_kws_score();
}
inline void SessionMsg::_internal_set_kws_score(::PROTOBUF_NAMESPACE_ID::int32 value) {
  
  kws_score_ = value;
}
inline void SessionMsg::set_kws_score(::PROTOBUF_NAMESPACE_ID::int32 value) {
  _internal_set_kws_score(value);
  // @@protoc_insertion_point(field_set:thead.voice.proto.SessionMsg.kws_score)
}

// -------------------------------------------------------------------

// RecordMsg

// .thead.voice.proto.RecordCmd cmd = 1;
inline void RecordMsg::clear_cmd() {
  cmd_ = 0;
}
inline ::thead::voice::proto::RecordCmd RecordMsg::_internal_cmd() const {
  return static_cast< ::thead::voice::proto::RecordCmd >(cmd_);
}
inline ::thead::voice::proto::RecordCmd RecordMsg::cmd() const {
  // @@protoc_insertion_point(field_get:thead.voice.proto.RecordMsg.cmd)
  return _internal_cmd();
}
inline void RecordMsg::_internal_set_cmd(::thead::voice::proto::RecordCmd value) {
  
  cmd_ = value;
}
inline void RecordMsg::set_cmd(::thead::voice::proto::RecordCmd value) {
  _internal_set_cmd(value);
  // @@protoc_insertion_point(field_set:thead.voice.proto.RecordMsg.cmd)
}

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__
// -------------------------------------------------------------------


// @@protoc_insertion_point(namespace_scope)

}  // namespace proto
}  // namespace voice
}  // namespace thead

PROTOBUF_NAMESPACE_OPEN

template <> struct is_proto_enum< ::thead::voice::proto::SessionCmd> : ::std::true_type {};
template <> struct is_proto_enum< ::thead::voice::proto::RecordCmd> : ::std::true_type {};

PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)

#include <google/protobuf/port_undef.inc>
#endif  // GOOGLE_PROTOBUF_INCLUDED_GOOGLE_PROTOBUF_INCLUDED_voice_2eproto
