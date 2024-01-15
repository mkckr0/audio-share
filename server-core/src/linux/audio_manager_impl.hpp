/*
   Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef _AUDIO_MANAGER_HPP
#define _AUDIO_MANAGER_HPP

#ifdef linux

class network_manager;

struct pw_main_loop;
struct pw_context;
struct pw_core;
struct roundtrip;

namespace detail {

class audio_manager_impl {
protected:
    audio_manager_impl();
    ~audio_manager_impl();

    struct pw_main_loop* _loop;
    struct pw_context* _context;
    struct pw_core* _core;
    struct roundtrip* _roundtrip;
};

} // namespace detail

#endif // linux
#endif // !_AUDIO_MANAGER_HPP