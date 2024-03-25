// (c)2024, Arthur van Hoff, Artfahrt Inc.
#define USE_TENSORFLOW
#ifdef USE_TENSORFLOW
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <esp_heap_caps.h>
#include "card_model.h"
#include "deal.h"
#include "camera.h"

bool initialized = false;
const tflite::Model* model = NULL;
constexpr int kTensorArenaSize = 50*120 + 100 + 100*1024;
static uint8_t *tensor_arena = NULL;
tflite::MicroInterpreter* interpreter = NULL;
TfLiteTensor* input = NULL;

void report_error() {
    dprintf("ERROR");
}

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
        dprintf("predict: got model, version=%d", model->version());
        tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (tensor_arena == NULL) {
            dprintf("predict: alloc failed, %d bytes", kTensorArenaSize);
            model = NULL;
            return CARD_FAIL;
        }
        dprintf("predict: got mem=%d", kTensorArenaSize);
        static tflite::MicroMutableOpResolver<5> micro_op_resolver;
        micro_op_resolver.AddMaxPool2D();
        micro_op_resolver.AddConv2D();
        micro_op_resolver.AddReshape();
        micro_op_resolver.AddTranspose();
        micro_op_resolver.AddFullyConnected();
        dprintf("predict: MicroMutableOpResolver");

        static tflite::MicroErrorReporter micro_error_reporter;

        static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize, &micro_error_reporter);
        interpreter = &static_interpreter;
        dprintf("predict: MicroInterpreter");

        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk) {
            dprintf("predict: alloc tensors failed");
            model = NULL;
            return CARD_FAIL;
        }
        input = interpreter->input(0);
        dprintf("predict: model ready");
        dprintf("predict: input bytes=%d, type=%d, %s", input->bytes, input->type, TfLiteTypeGetName(input->type));
        for (int i = 0 ; i < input->dims->size ; i++) {
            dprintf("predict: input dim[%d] = %d", i, input->dims->data[i]);
        }

    }
    if (model == NULL) {
        return CARD_FAIL;
    }

    unsigned long tm = millis();
    // copy the image
    float *dp = input->data.f;
    for (int r = 0 ; r < img.height ; r++) {
        const pixel *sp = img.addr(0, r);
        for (int c = 0 ; c < img.width ; c++) {
            *dp++ = *sp++ / 255.0f - 0.5f;
        }
    }
    if (kTfLiteOk != interpreter->Invoke()) {
        dprintf("predict: invoke failed");
        return CARD_FAIL;
    }
    TfLiteTensor *output = interpreter->output(0);
    //dprintf("predict: output bytes=%d, type=%d, %s", output->bytes, output->type, TfLiteTypeGetName(output->type));
    //for (int i = 0 ; i < output->dims->size ; i++) {
    //    dprintf("predict: output dim[%d] = %d", i, output->dims->data[i]);
    //}
    //for (int i = 0 ; i < 53 ; i++) {
    //    dprintf("predict: output[%d] = %f", i, output->data.f[i]);
    //}
    int s = 0;
    float sbest = output->data.f[0];
    for (int i = 1 ; i <= NSUITS ; i++) {
        if (output->data.f[i] > sbest) {
            sbest = output->data.f[i];
            s = i;
        }
    }
    int c = 0;
    float cbest = output->data.f[NSUITS];
    for (int i = 0 ; i <= HANDSIZE ; i++) {
        if (output->data.f[NSUITS + 1 + i] > cbest) {
            cbest = output->data.f[NSUITS + 1 + i];
            c = i;
        }
    }
    int cs = s * HANDSIZE + c;
    if (s > NSUITS || c > HANDSIZE) {
        cs = CARD_EMPTY;
    }
    tm = millis() - tm;
    dprintf("predict: best index=%d, %s in %lums", cs, full_name(cs), tm);

    return cs;
}
#else
#include "camera.h"

int Camera::predict(const Image &img)
{
    return CARD_NULL;
}
#endif