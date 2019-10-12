//
// Copyright (c) 2018-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/json
//

#ifndef BOOST_JSON_IMPL_STRING_HPP
#define BOOST_JSON_IMPL_STRING_HPP

#include <boost/core/ignore_unused.hpp>
#include <algorithm>
#include <utility>
#include <string>

namespace boost {
namespace json {

//------------------------------------------------------------------------------

auto
string::
impl::
growth(
    size_type new_size,
    size_type capacity) ->
        size_type
{
    if(new_size > max_size_)
        BOOST_THROW_EXCEPTION(
            std::length_error(
                "size > max_size()"));
    new_size |= mask_;
    if( new_size > max_size_)
        return max_size_;
    if( capacity >
        max_size_ - capacity / 2)
        return max_size_; // overflow
    return (std::max)(new_size,
        capacity + capacity / 2);
}

void
string::
impl::
destroy(
    storage_ptr const& sp)
{
    if(! in_sbo())
        sp->deallocate(
            p, capacity + 1, 1);
}

void
string::
impl::
construct() noexcept
{
    size = 0;
    capacity = sizeof(buf) - 1;
    buf[0] = 0;
}

char*
string::
impl::
construct(
    size_type new_size,
    storage_ptr const& sp)
{
    if(new_size < sizeof(buf))
    {
        // SBO
        capacity = sizeof(buf) - 1;
        return buf;
    }

    capacity = growth(new_size,
        sizeof(buf) - 1);
    p = static_cast<char*>(
        sp->allocate(capacity + 1, 1));
    return p;
}

template<class InputIt>
void
string::
impl::
construct(
    InputIt first,
    InputIt last,
    storage_ptr const& sp,
    std::forward_iterator_tag)
{
    auto const n =
        std::distance(first, last);
    auto dest = construct(n, sp);
    while(first != last)
        *dest++ = *first++;
    *dest++ = 0;
    size = n;
}

template<class InputIt>
void
string::
impl::
construct(
    InputIt first,
    InputIt last,
    storage_ptr const& sp,
    std::input_iterator_tag)
{
    struct undo
    {
        impl& s;
        storage_ptr const& sp;
        bool commit;

        ~undo()
        {
            if(! commit)
                s.destroy(sp);
        }
    };

    undo u{*this, sp, false};
    auto dest = construct(1, sp);
    size = 1;
    *dest++ = *first++;
    while(first != last)
    {
        if(size < capacity)
            ++size;
        else
            dest = append(1, sp);
        *dest++ = *first++;
    }
    *dest = 0;
    u.commit = true;
}

char*
string::
impl::
assign(
    size_type new_size,
    storage_ptr const& sp)
{
    if(new_size > capacity)
    {
        impl tmp;
        tmp.construct(growth(
            new_size, capacity), sp);
        destroy(sp);
        *this = tmp;
    }
    term(new_size);
    return data();
}

char*
string::
impl::
append(
    size_type n,
    storage_ptr const& sp)
{
    if(n > max_size_ - size)
        BOOST_THROW_EXCEPTION(
            std::length_error(
                "size > max_size()"));
    if(n <= capacity - size)
    {
        term(size + n);
        return end() - n;
    }
    impl tmp;
    traits_type::copy(
        tmp.construct(growth(
            size + n, capacity), sp),
        data(), size);
    tmp.term(size + n);
    destroy(sp);
    *this = tmp;
    return end() - n;
}

char*
string::
impl::
insert(
    size_type pos,
    size_type n,
    storage_ptr const& sp)
{
    if(pos > size)
        BOOST_THROW_EXCEPTION(
            std::out_of_range(
                "pos > size()"));
    if(n <= capacity - size)
    {
        auto const dest =
            data() + pos;
        traits_type::move(
            dest + n,
            dest,
            size + 1 - pos);
        size += n;
        return dest;
    }
    if(n > max_size_ - size)
        BOOST_THROW_EXCEPTION(
            std::length_error(
                "size > max_size()"));
    impl tmp;
    tmp.construct(growth(
        size + n, capacity), sp);
    tmp.size = size + n;
    traits_type::copy(
        tmp.data(),
        data(),
        pos);
    traits_type::copy(
        tmp.data() + pos + n,
        data() + pos,
        size + 1 - pos);
    destroy(sp);
    *this = tmp;
    return data() + pos;
}

void
string::
impl::
unalloc(storage_ptr const& sp) noexcept
{
    BOOST_ASSERT(size < sizeof(buf));
    BOOST_ASSERT(! in_sbo());
    auto const p_ = p;
    traits_type::copy(
        buf, data(), size + 1);
    sp->deallocate(
        p_, capacity + 1, 1);
    capacity = sizeof(buf) - 1;
}

//------------------------------------------------------------------------------

template<class InputIt, class>
string::
string(
    InputIt first,
    InputIt last,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    s_.construct(
        first, last, sp_,
        iter_cat<InputIt>{});
}

template<class InputIt, class>
string&
string::
assign(
    InputIt first,
    InputIt last)
{
    assign(first, last,
        iter_cat<InputIt>{});
    return *this;
}

template<class InputIt, class>
string&
string::
append(InputIt first, InputIt last)
{
    append(first, last,
        iter_cat<InputIt>{});
    return *this;
}

template<class InputIt, class>
auto
string::
insert(
    const_iterator pos,
    InputIt first,
    InputIt last) ->
        iterator
{
    struct cleanup
    {
        impl& s;
        storage_ptr const& sp;

        ~cleanup()
        {
            s.destroy(sp);
        }
    };

    impl tmp;
    tmp.construct(first, last,
        detail::global_storage(),
        iter_cat<InputIt>{});
    cleanup c{tmp,
        detail::global_storage()};
    auto const off = pos - s_.data();
    traits_type::copy(
        s_.insert(off, tmp.size, sp_),
        tmp.data(),
        tmp.size);
    return s_.data() + off;
}

//------------------------------------------------------------------------------

template<class InputIt>
void
string::
assign(
    InputIt first,
    InputIt last,
    std::forward_iterator_tag)
{
    auto dest = s_.assign(
        std::distance(first, last), sp_);
    while(first != last)
        *dest++ = *first++;
}

template<class InputIt>
void
string::
assign(
    InputIt first,
    InputIt last,
    std::input_iterator_tag)
{
    if(first == last)
    {
        s_.term(0);
        return;
    }
    impl tmp;
    tmp.construct(
        first, last, sp_,
        std::input_iterator_tag{});
    s_.destroy(sp_);
    s_ = tmp;
}

template<class InputIt>
void
string::
append(
    InputIt first,
    InputIt last,
    std::forward_iterator_tag)
{
    auto const n =
        std::distance(first, last);
    std::copy(first, last, s_.append(n, sp_));
}

template<class InputIt>
void
string::
append(
    InputIt first,
    InputIt last,
    std::input_iterator_tag)
{
    struct cleanup
    {
        impl& s;
        storage_ptr const& sp;

        ~cleanup()
        {
            s.destroy(sp);
        }
    };

    impl tmp;
    tmp.construct(first, last,
        detail::global_storage(),
        std::input_iterator_tag{});
    cleanup c{tmp,
        detail::global_storage()};
    traits_type::copy(
        s_.append(tmp.size, sp_),
        tmp.data(), tmp.size);
}

} // json
} // boost

#endif