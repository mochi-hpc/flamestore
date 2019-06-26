#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/op_kernel.h"
#include <iostream>
#include <functional>
#include "../client.hpp"

using namespace tensorflow;
using shape_inference::DimensionHandle;
using shape_inference::InferenceContext;
using shape_inference::ShapeHandle;

REGISTER_OP("ReadModel")
    .Attr("client: string")
    .Attr("provider_addr: string")
    .Attr("provider_id: int")
    .Attr("model_name: string")
    .Attr("model_signature: string")
    .Input("tensors: Ref(N * T)")
    .Attr("T: type")
    .Attr("N: int")
    .SetIsStateful()
    .SetShapeFn([](::tensorflow::shape_inference::InferenceContext* c) {
            return Status::OK();
    });


class ReadModelOp : public OpKernel {

private:
    
    std::string          m_model_name;
    std::string          m_model_signature;
    flamestore_client*         m_client;
    flamestore_provider_handle m_provider_handle;

public:

    explicit ReadModelOp(OpKernelConstruction* context)
    : OpKernel(context) {
        OP_REQUIRES_OK(context, context->GetAttr("model_name",      &m_model_name));
        std::string client_id;
        OP_REQUIRES_OK(context, context->GetAttr("client",          &client_id));
        m_client = flamestore_client::from_id(client_id);
        std::string provider_addr;
        OP_REQUIRES_OK(context, context->GetAttr("provider_addr",   &provider_addr));
        int provider_id;
        OP_REQUIRES_OK(context, context->GetAttr("provider_id",     &provider_id));
        m_provider_handle = m_client->lookup(provider_addr, provider_id);
        for(int i=0; i < context->num_inputs(); i++) {
            OP_REQUIRES(context, IsRefType(context->input_type(i)),
                errors::InvalidArgument("input tensors needs to be a ref type"));
        }
        OP_REQUIRES_OK(context, context->GetAttr("model_signature", &m_model_signature));
    }

    void Compute(OpKernelContext* context) override {

        unsigned n = context->num_inputs();
        std::vector<std::pair<void*, size_t>> segments(n);
        for(unsigned i=0; i < n; i++) {
            auto tensor = context->mutable_input(i, false);
            segments[i].first  = static_cast<void*>(
                    const_cast<char*>(tensor.tensor_data().data()));
            segments[i].second = tensor.tensor_data().size();
        }
        
        auto local_bulk = m_client->engine().expose(segments, tl::bulk_mode::write_only);

        auto status = m_client->read_model_data(
                                    m_provider_handle,
                                    m_model_name,
                                    m_model_signature,
                                    local_bulk);
        OP_REQUIRES(context, status.first == 0,
                errors::Internal(status.second));
    }

};

REGISTER_KERNEL_BUILDER(Name("ReadModel").Device(DEVICE_CPU), ReadModelOp);
