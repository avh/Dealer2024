// (c)2024, Arthur van Hoff, Artfahrt Inc.

#ifdef USE_TENSORFLOW
#include <tensorflow/lite/micro/micro_interpreter.h>
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

        static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize);
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
    int8_t *dp = input->data.int8;
    for (int r = 0 ; r < img.height ; r++) {
        const pixel *sp = img.addr(0, r);
        for (int c = 0 ; c < img.width ; c++) {
            *dp++ = *sp++ - 128;
        }
    }
    if (kTfLiteOk != interpreter->Invoke()) {
        dprintf("predict: invoke failed");
        return CARD_FAIL;
    }
    TfLiteTensor *output = interpreter->output(0);
    if (false) {
        dprintf("predict: output bytes=%d, type=%d, %s", output->bytes, output->type, TfLiteTypeGetName(output->type));
        for (int i = 0 ; i < output->dims->size ; i++) {
            dprintf("predict: output dim[%d] = %d", i, output->dims->data[i]);
        }
        for (int i = 0 ; i < output->dims->data[1] ; i++) {
            dprintf("predict: output[%d] = %d", i, output->data.int8[i]);
        }
    }
    int s = 0;
    int sbest = output->data.int8[0];
    for (int i = 1 ; i < NSUITS+2 ; i++) {
        if (output->data.int8[i] > sbest) {
            sbest = output->data.int8[i];
            s = i;
        }
    }
    int c = 0;
    int cbest = output->data.int8[NSUITS+2];
    for (int i = 1 ; i < HANDSIZE+2 ; i++) {
        if (output->data.int8[NSUITS+2 + i] > cbest) {
            cbest = output->data.int8[NSUITS+2 + i];
            c = i;
        }
    }
    int cs = s * HANDSIZE + c;
    if (s == NSUITS+1 || c == HANDSIZE+1) {
        cs = CARD_MOTION;
    } else if (s == NSUITS || c == HANDSIZE) {
        cs = CARD_EMPTY;
    }
    tm = millis() - tm;
    //dprintf("predict: best index=%d, %s in %lums", cs, full_name(cs), tm);

    return cs;
}
#else
#include "camera.h"

int Camera::predict(const Image &img)
{
    return CARD_NULL;
}
#endif