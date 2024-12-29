#pragma once

#include "format.hpp"

#define OPENSSL_API_COMPAT 30000

extern "C" {
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
}

namespace wshttp
{
    inline constexpr size_t RSA_KEYSIZE{4096};

    template <size_t N>
    struct datum
    {
      private:
        std::array<uint8_t, N> buf{};

        datum(const uint8_t* data, size_t sz) { write(data, sz); }

      public:
        datum() = default;

        template <enc::basic_char T>
        datum(const_span<T> data) : datum{reinterpret_cast<const uint8_t*>(data.data()), data.size()}
        {}

        datum(const datum& other) : datum{other.buf.data(), other.buf.size()} {}

        datum& operator=(const datum& other)
        {
            buf = other.buf;
            return *this;
        }

        inline void write(const uint8_t* data, size_t sz)
        {
            if (sz != N)
                throw std::invalid_argument{"Datum size must be {}"_format(N)};

            std::memcpy(buf.data(), data, sz);
        }

        template <enc::basic_char T = uint8_t>
        const_span<T> span() const
        {
            return {reinterpret_cast<const T*>(buf.data()), buf.size()};
        }

        explicit operator bool() const { return !buf.empty(); }

        bool operator<=>(const datum& other) const { return buf <=> other.buf; }
    };

    namespace deleters
    {
        struct _evp
        {
            inline void operator()(::EVP_PKEY* p) const { return ::EVP_PKEY_free(p); }
        };

        struct _x509
        {
            inline void operator()(::X509* x) const { return ::X509_free(x); }
        };
    }  // namespace deleters

    using evp_pkey_ptr = std::unique_ptr<::EVP_PKEY, deleters::_evp>;

    using x509_ptr = std::unique_ptr<::X509, deleters::_x509>;

    struct x509_cert_keypair
    {
      private:
        evp_pkey_ptr pk;
        x509_ptr x;

      public:
        // x509_cert_keypair() {}

        void _init_internals();
    };
}  // namespace wshttp
