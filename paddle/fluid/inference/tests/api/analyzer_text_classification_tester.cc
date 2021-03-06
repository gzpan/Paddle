// Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "paddle/fluid/inference/tests/api/tester_helper.h"

namespace paddle {
namespace inference {

struct DataReader {
  explicit DataReader(const std::string &path)
      : file(new std::ifstream(path)) {}

  bool NextBatch(std::vector<PaddleTensor> *input, int batch_size) {
    PADDLE_ENFORCE_EQ(batch_size, 1);
    std::string line;
    PaddleTensor tensor;
    tensor.dtype = PaddleDType::INT64;
    tensor.lod.emplace_back(std::vector<size_t>({0}));
    std::vector<int64_t> data;

    for (int i = 0; i < batch_size; i++) {
      if (!std::getline(*file, line)) return false;
      inference::split_to_int64(line, ' ', &data);
    }
    tensor.lod.front().push_back(data.size());

    tensor.data.Resize(data.size() * sizeof(int64_t));
    memcpy(tensor.data.data(), data.data(), data.size() * sizeof(int64_t));
    tensor.shape.push_back(data.size());
    tensor.shape.push_back(1);
    input->assign({tensor});
    return true;
  }

  std::unique_ptr<std::ifstream> file;
};

void SetConfig(AnalysisConfig *cfg) {
  cfg->model_dir = FLAGS_infer_model;
  cfg->use_gpu = false;
  cfg->device = 0;
  cfg->specify_input_name = true;
  cfg->enable_ir_optim = true;
}

void SetInput(std::vector<std::vector<PaddleTensor>> *inputs) {
  std::vector<PaddleTensor> input_slots;
  DataReader reader(FLAGS_infer_data);
  int num_batches = 0;
  while (reader.NextBatch(&input_slots, FLAGS_batch_size)) {
    (*inputs).emplace_back(input_slots);
    ++num_batches;
    if (!FLAGS_test_all_data) return;
  }
  LOG(INFO) << "total number of samples: " << num_batches * FLAGS_batch_size;
}

// Easy for profiling independently.
TEST(Analyzer_Text_Classification, profile) {
  AnalysisConfig cfg;
  SetConfig(&cfg);
  std::vector<PaddleTensor> outputs;

  std::vector<std::vector<PaddleTensor>> input_slots_all;
  SetInput(&input_slots_all);
  TestPrediction(cfg, input_slots_all, &outputs, FLAGS_num_threads);

  if (FLAGS_num_threads == 1) {
    // Get output
    LOG(INFO) << "get outputs " << outputs.size();
    for (auto &output : outputs) {
      LOG(INFO) << "output.shape: " << to_string(output.shape);
      // no lod ?
      CHECK_EQ(output.lod.size(), 0UL);
      LOG(INFO) << "output.dtype: " << output.dtype;
      std::stringstream ss;
      for (int i = 0; i < 5; i++) {
        ss << static_cast<float *>(output.data.data())[i] << " ";
      }
      LOG(INFO) << "output.data summary: " << ss.str();
      // one batch ends
    }
  }
}

// Compare result of NativeConfig and AnalysisConfig
TEST(Analyzer_Text_Classification, compare) {
  AnalysisConfig cfg;
  SetConfig(&cfg);

  std::vector<std::vector<PaddleTensor>> input_slots_all;
  SetInput(&input_slots_all);
  CompareNativeAndAnalysis(cfg, input_slots_all);
}

}  // namespace inference
}  // namespace paddle
