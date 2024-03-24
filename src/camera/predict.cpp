// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "camera.h"

#ifdef USE_TENSORFLOW
#include "card_model.h"
#include <tensorflow/lite/micro/micro_interpreter.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include <esp_heap_caps.h>

bool initialized = false;
const tflite::Model* model = NULL;
constexpr int kTensorArenaSize = 50*120 + 100 + 40*1024;
static uint8_t *tensor_arena = NULL;
tflite::MicroInterpreter* interpreter = NULL;
TfLiteTensor* input = NULL;

int Camera::predict(const Image &img)
{
    if (!initialized) {
        initialized = true;
        dprintf("predict: initialize tensorflow model");

        model = tflite::GetModel(card_model_tflite);
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            dprintf("predict: wrong model version, got %d, expected %d", model->version(), TFLITE_SCHEMA_VERSION);
            return CARD_FAIL;
        }
        tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (tensor_arena == NULL) {
            dprintf("predict: alloc failed, %d bytes", kTensorArenaSize);
            model = NULL;
            return CARD_FAIL;
        }
        static tflite::MicroMutableOpResolver<5> micro_op_resolver;
        micro_op_resolver.AddMaxPool2D();
        micro_op_resolver.AddConv2D();

        static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize, NULL);
        interpreter = &static_interpreter;

        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk) {
            dprintf("predict: alloc tensors");
            model = NULL;
            return CARD_FAIL;
        }
        input = interpreter->input(0);
        dprintf("predict: model ready");
        for (int i = 0 ; i < input->dims->size ; i++) {
            dprintf("predict: input dim[%d] = %d", i, input->dims->data[i]);
        }
    }
    if (model == NULL) {
        return CARD_FAIL;
    }
    // copy the image
    for (int r = 0 ; r < img.height ; r++) {
        bcopy(img.addr(0, r), input->data.int8 + r * img.width, img.width);
    }
    if (kTfLiteOk != interpreter->Invoke()) {
        dprintf("predict: invoke failed");
        return CARD_FAIL;
    }
    TfLiteTensor* output = interpreter->output(0);
    for (int i = 0 ; i < output->dims->size ; i++) {
        dprintf("predict: output dim[%d] = %d", i, output->dims->data[i]);
    }

    return CARD_FAIL;
}
#else
int Camera::predict(const Image &img)
{
    return CARD_NULL;
}
#endif