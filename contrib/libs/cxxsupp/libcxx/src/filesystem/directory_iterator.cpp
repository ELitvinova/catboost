//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "filesystem"
#include "__config"
#if defined(_LIBCPP_WIN32API)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <errno.h>

#include "filesystem_common.h"

_LIBCPP_BEGIN_NAMESPACE_FILESYSTEM

namespace detail {
namespace {

#if !defined(_LIBCPP_WIN32API)

#if defined(DT_BLK)
template <class DirEntT, class = decltype(DirEntT::d_type)>
static file_type get_file_type(DirEntT* ent, int) {
  switch (ent->d_type) {
  case DT_BLK:
    return file_type::block;
  case DT_CHR:
    return file_type::character;
  case DT_DIR:
    return file_type::directory;
  case DT_FIFO:
    return file_type::fifo;
  case DT_LNK:
    return file_type::symlink;
  case DT_REG:
    return file_type::regular;
  case DT_SOCK:
    return file_type::socket;
  // Unlike in lstat, hitting "unknown" here simply means that the underlying
  // filesystem doesn't support d_type. Report is as 'none' so we correctly
  // set the cache to empty.
  case DT_UNKNOWN:
    break;
  }
  return file_type::none;
}
#endif // defined(DT_BLK)

template <class DirEntT>
static file_type get_file_type(DirEntT*, long) {
  return file_type::none;
}

static pair<string_view, file_type> posix_readdir(DIR* dir_stream,
                                                  error_code& ec) {
  struct dirent* dir_entry_ptr = nullptr;
  errno = 0; // zero errno in order to detect errors
  ec.clear();
  if ((dir_entry_ptr = ::readdir(dir_stream)) == nullptr) {
    if (errno)
      ec = capture_errno();
    return {};
  } else {
    return {dir_entry_ptr->d_name, get_file_type(dir_entry_ptr, 0)};
  }
}
#else
// defined(_LIBCPP_WIN32API)

static file_type get_file_type(const WIN32_FIND_DATAW& data) {
  if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT &&
      data.dwReserved0 == IO_REPARSE_TAG_SYMLINK)
    return file_type::symlink;
  if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    return file_type::directory;
  return file_type::regular;
}
static uintmax_t get_file_size(const WIN32_FIND_DATAW& data) {
  return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) + data.nFileSizeLow;
}
static file_time_type get_write_time(const WIN32_FIND_DATAW& data) {
  ULARGE_INTEGER tmp;
  const FILETIME& time = data.ftLastWriteTime;
  tmp.u.LowPart = time.dwLowDateTime;
  tmp.u.HighPart = time.dwHighDateTime;
  return file_time_type(file_time_type::duration(tmp.QuadPart));
}

#endif

} // namespace
} // namespace detail

using detail::ErrorHandler;

#if defined(_LIBCPP_WIN32API)
class __dir_stream {
public:
  __dir_stream() = delete;
  __dir_stream& operator=(const __dir_stream&) = delete;

  __dir_stream(__dir_stream&& __ds) noexcept : __stream_(__ds.__stream_),
                                               __root_(move(__ds.__root_)),
                                               __entry_(move(__ds.__entry_)) {
    __ds.__stream_ = INVALID_HANDLE_VALUE;
  }

  __dir_stream(const path& root, directory_options opts, error_code& ec)
      : __stream_(INVALID_HANDLE_VALUE), __root_(root) {
    if (root.native().empty()) {
      ec = make_error_code(errc::no_such_file_or_directory);
      return;
    }
    __stream_ = ::FindFirstFileW((root / "*").c_str(), &__data_);
    if (__stream_ == INVALID_HANDLE_VALUE) {
      ec = detail::make_windows_error(GetLastError());
      const bool ignore_permission_denied =
          bool(opts & directory_options::skip_permission_denied);
      if (ignore_permission_denied &&
          ec.value() == static_cast<int>(errc::permission_denied))
        ec.clear();
      return;
    }
    if (!assign())
      advance(ec);
  }

  ~__dir_stream() noexcept {
    if (__stream_ == INVALID_HANDLE_VALUE)
      return;
    close();
  }

  bool good() const noexcept { return __stream_ != INVALID_HANDLE_VALUE; }

  bool advance(error_code& ec) {
    while (::FindNextFileW(__stream_, &__data_)) {
      if (assign())
        return true;
    }
    close();
    return false;
  }

  bool assign() {
    if (!wcscmp(__data_.cFileName, L".") || !wcscmp(__data_.cFileName, L".."))
      return false;
    // FIXME: Cache more of this
    //directory_entry::__cached_data cdata;
    //cdata.__type_ = get_file_type(__data_);
    //cdata.__size_ = get_file_size(__data_);
    //cdata.__write_time_ = get_write_time(__data_);
    __entry_.__assign_iter_entry(
        __root_ / __data_.cFileName,
        directory_entry::__create_iter_result(detail::get_file_type(__data_)));
    return true;
  }

private:
  error_code close() noexcept {
    error_code ec;
    if (!::FindClose(__stream_))
      ec = detail::make_windows_error(GetLastError());
    __stream_ = INVALID_HANDLE_VALUE;
    return ec;
  }

  HANDLE __stream_{INVALID_HANDLE_VALUE};
  WIN32_FIND_DATAW __data_;

public:
  path __root_;
  directory_entry __entry_;
};
#else
class __dir_stream {
public:
  __dir_stream() = delete;
  __dir_stream& operator=(const __dir_stream&) = delete;

  __dir_stream(__dir_stream&& other) noexcept : __stream_(other.__stream_),
                                                __root_(move(other.__root_)),
                                                __entry_(move(other.__entry_)) {
    other.__stream_ = nullptr;
  }

  __dir_stream(const path& root, directory_options opts, error_code& ec)
      : __stream_(nullptr), __root_(root) {
    if ((__stream_ = ::opendir(root.c_str())) == nullptr) {
      ec = detail::capture_errno();
      const bool allow_eacess =
          bool(opts & directory_options::skip_permission_denied);
      if (allow_eacess && ec.value() == EACCES)
        ec.clear();
      return;
    }
    advance(ec);
  }

  ~__dir_stream() noexcept {
    if (__stream_)
      close();
  }

  bool good() const noexcept { return __stream_ != nullptr; }

  bool advance(error_code& ec) {
    while (true) {
      auto str_type_pair = detail::posix_readdir(__stream_, ec);
      auto& str = str_type_pair.first;
      if (str == "." || str == "..") {
        continue;
      } else if (ec || str.empty()) {
        close();
        return false;
      } else {
        __entry_.__assign_iter_entry(
            __root_ / str,
            directory_entry::__create_iter_result(str_type_pair.second));
        return true;
      }
    }
  }

private:
  error_code close() noexcept {
    error_code m_ec;
    if (::closedir(__stream_) == -1)
      m_ec = detail::capture_errno();
    __stream_ = nullptr;
    return m_ec;
  }

  DIR* __stream_{nullptr};

public:
  path __root_;
  directory_entry __entry_;
};
#endif

// directory_iterator

directory_iterator::directory_iterator(const path& p, error_code* ec,
                                       directory_options opts) {
  ErrorHandler<void> err("directory_iterator::directory_iterator(...)", ec, &p);

  error_code m_ec;
  __imp_ = make_shared<__dir_stream>(p, opts, m_ec);
  if (ec)
    *ec = m_ec;
  if (!__imp_->good()) {
    __imp_.reset();
    if (m_ec)
      err.report(m_ec);
  }
}

directory_iterator& directory_iterator::__increment(error_code* ec) {
  _LIBCPP_ASSERT(__imp_, "Attempting to increment an invalid iterator");
  ErrorHandler<void> err("directory_iterator::operator++()", ec);

  error_code m_ec;
  if (!__imp_->advance(m_ec)) {
    path root = move(__imp_->__root_);
    __imp_.reset();
    if (m_ec)
      err.report(m_ec, "at root " PATH_CSTR_FMT, root.c_str());
  }
  return *this;
}

directory_entry const& directory_iterator::__dereference() const {
  _LIBCPP_ASSERT(__imp_, "Attempting to dereference an invalid iterator");
  return __imp_->__entry_;
}

// recursive_directory_iterator

struct recursive_directory_iterator::__shared_imp {
  stack<__dir_stream> __stack_;
  directory_options __options_;
};

recursive_directory_iterator::recursive_directory_iterator(
    const path& p, directory_options opt, error_code* ec)
    : __imp_(nullptr), __rec_(true) {
  ErrorHandler<void> err("recursive_directory_iterator", ec, &p);

  error_code m_ec;
  __dir_stream new_s(p, opt, m_ec);
  if (m_ec)
    err.report(m_ec);
  if (m_ec || !new_s.good())
    return;

  __imp_ = make_shared<__shared_imp>();
  __imp_->__options_ = opt;
  __imp_->__stack_.push(move(new_s));
}

void recursive_directory_iterator::__pop(error_code* ec) {
  _LIBCPP_ASSERT(__imp_, "Popping the end iterator");
  if (ec)
    ec->clear();
  __imp_->__stack_.pop();
  if (__imp_->__stack_.size() == 0)
    __imp_.reset();
  else
    __advance(ec);
}

directory_options recursive_directory_iterator::options() const {
  return __imp_->__options_;
}

int recursive_directory_iterator::depth() const {
  return __imp_->__stack_.size() - 1;
}

const directory_entry& recursive_directory_iterator::__dereference() const {
  return __imp_->__stack_.top().__entry_;
}

recursive_directory_iterator&
recursive_directory_iterator::__increment(error_code* ec) {
  if (ec)
    ec->clear();
  if (recursion_pending()) {
    if (__try_recursion(ec) || (ec && *ec))
      return *this;
  }
  __rec_ = true;
  __advance(ec);
  return *this;
}

void recursive_directory_iterator::__advance(error_code* ec) {
  ErrorHandler<void> err("recursive_directory_iterator::operator++()", ec);

  const directory_iterator end_it;
  auto& stack = __imp_->__stack_;
  error_code m_ec;
  while (stack.size() > 0) {
    if (stack.top().advance(m_ec))
      return;
    if (m_ec)
      break;
    stack.pop();
  }

  if (m_ec) {
    path root = move(stack.top().__root_);
    __imp_.reset();
    err.report(m_ec, "at root " PATH_CSTR_FMT, root.c_str());
  } else {
    __imp_.reset();
  }
}

bool recursive_directory_iterator::__try_recursion(error_code* ec) {
  ErrorHandler<void> err("recursive_directory_iterator::operator++()", ec);

  bool rec_sym = bool(options() & directory_options::follow_directory_symlink);

  auto& curr_it = __imp_->__stack_.top();

  bool skip_rec = false;
  error_code m_ec;
  if (!rec_sym) {
    file_status st(curr_it.__entry_.__get_sym_ft(&m_ec));
    if (m_ec && status_known(st))
      m_ec.clear();
    if (m_ec || is_symlink(st) || !is_directory(st))
      skip_rec = true;
  } else {
    file_status st(curr_it.__entry_.__get_ft(&m_ec));
    if (m_ec && status_known(st))
      m_ec.clear();
    if (m_ec || !is_directory(st))
      skip_rec = true;
  }

  if (!skip_rec) {
    __dir_stream new_it(curr_it.__entry_.path(), __imp_->__options_, m_ec);
    if (new_it.good()) {
      __imp_->__stack_.push(move(new_it));
      return true;
    }
  }
  if (m_ec) {
    const bool allow_eacess =
        bool(__imp_->__options_ & directory_options::skip_permission_denied);
    if (m_ec.value() == EACCES && allow_eacess) {
      if (ec)
        ec->clear();
    } else {
      path at_ent = move(curr_it.__entry_.__p_);
      __imp_.reset();
      err.report(m_ec, "attempting recursion into " PATH_CSTR_FMT,
                 at_ent.c_str());
    }
  }
  return false;
}

_LIBCPP_END_NAMESPACE_FILESYSTEM
