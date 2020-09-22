/**
 *
 *  Copyright 2016-2020 Netflix, Inc.
 *
 *     Licensed under the BSD+Patent License (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         https://opensource.org/licenses/BSDplusPatent
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include <errno.h>
#include <string.h>

#include "feature_collector.h"
#include "feature_extractor.h"

#include "mem.h"
#include "ansnr.h"
#include "picture_copy.h"

typedef struct AnsnrState {
    size_t float_stride;
    float *ref;
    float *dist;
    double peak;
    double psnr_max;
} AnsnrState;

static int init(VmafFeatureExtractor *fex, enum VmafPixelFormat pix_fmt,
                unsigned bpc, unsigned w, unsigned h)
{
    AnsnrState *s = fex->priv;
    s->float_stride = ALIGN_CEIL(w * sizeof(float));
    s->ref = aligned_malloc(s->float_stride * h, 32);
    if (!s->ref) goto fail;
    s->dist = aligned_malloc(s->float_stride * h, 32);
    if (!s->dist) goto free_ref;

    s->peak = bpc == 8 ? 255.0 : 255.75;
    s->psnr_max = bpc == 8 ? 60.0 : 72.0;

    return 0;

    free_ref:
    free(s->ref);
    fail:
    return -ENOMEM;
}

static int extract(VmafFeatureExtractor *fex,
                   VmafPicture *ref_pic, VmafPicture *ref_pic_90,
                   VmafPicture *dist_pic, VmafPicture *dist_pic_90,
                   unsigned index, VmafFeatureCollector *feature_collector)
{
    AnsnrState *s = fex->priv;
    int err = 0;

    (void) ref_pic_90;
    (void) dist_pic_90;

    picture_copy(s->ref, s->float_stride, ref_pic, -128, ref_pic->bpc);
    picture_copy(s->dist, s->float_stride, dist_pic, -128, dist_pic->bpc);

    double score, score_psnr;
    err = compute_ansnr(s->ref, s->dist, ref_pic->w[0], ref_pic->h[0],
                        s->float_stride, s->float_stride, &score, &score_psnr,
                        s->peak, s->psnr_max);

    if (err) return err;
    err = vmaf_feature_collector_append(feature_collector, "float_ansnr",
                                        score, index);
    if (err) return err;
    err = vmaf_feature_collector_append(feature_collector, "float_anpsnr",
                                        score_psnr, index);
    if (err) return err;
    return 0;
}

static int close(VmafFeatureExtractor *fex)
{
    AnsnrState *s = fex->priv;
    if (s->ref) aligned_free(s->ref);
    if (s->dist) aligned_free(s->dist);
    return 0;
}

static const char *provided_features[] = {
        "float_ansnr",
        NULL
};

VmafFeatureExtractor vmaf_fex_float_ansnr = {
        .name = "float_ansnr",
        .init = init,
        .extract = extract,
        .close = close,
        .priv_size = sizeof(AnsnrState),
        .provided_features = provided_features,
};
