/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_CUDA_CUDA_ERROR_H_
#define MCF_CUDA_CUDA_ERROR_H_

#include <stdexcept>
#include <string>

namespace mcf
{
namespace cuda
{

/**
 * Exception indicating a CUDA error
 */
class cuda_error : public ::std::runtime_error
{
public:
    /**
     * Constructor
     *
     * @param what  Description of the exception
     */
    explicit cuda_error(const char* what = "Unspecified reason") : ::std::runtime_error(what) {}

    /**
     * Constructor
     *
     * @param what  Description of the exception
     */
    explicit cuda_error(const std::string& what = std::string("Unspecified reason"))
    : ::std::runtime_error(what)
    {
    }

    /**
     * Destructor
     */
    virtual ~cuda_error() {}
};

} // namespace cuda
} // namespace mcf

#endif /* MCF_CUDA_CUDA_ERROR_H_ */
