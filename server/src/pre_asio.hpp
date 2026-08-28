#ifndef AUDIOSTREAM_PRE_ASIO_HPP
#define AUDIOSTREAM_PRE_ASIO_HPP

#ifdef _WINDOWS
    #include <sdkddkver.h>
    #if defined(_MSC_VER) && !defined(ASIO_HAS_CO_AWAIT)
        #define ASIO_HAS_CO_AWAIT
    #endif
#endif

#endif // AUDIOSTREAM_PRE_ASIO_HPP
