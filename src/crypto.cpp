#include "crypto.hpp"

#include "internal.hpp"

namespace wshttp
{
    void x509_cert_keypair::_init_internals()
    {
        pk.reset(EVP_RSA_gen(RSA_KEYSIZE));

        if (!pk)
            throw std::runtime_error{"Failed to create RSA keypair object: {}"_format(detail::current_error())};

        x.reset(X509_new());

        if (!x)
            throw std::runtime_error{"Failed to create X509 cert object: {}"_format(detail::current_error())};

        X509_set_version(x.get(), NID_X509);
    }
}  // namespace wshttp
