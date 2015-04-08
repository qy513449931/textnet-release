#ifndef TEXTNET_ADAGRAD_UPDATER_INL_HPP_
#define TEXTNET_ADAGRAD_UPDATER_INL_HPP_

#include <iostream>
#include <mshadow/tensor.h>
#include "./updater.h"

namespace textnet {
namespace updater {

template<typename xpu, int dim>
class AdagradUpdater : public Updater<xpu, dim>{
 public:
  AdagradUpdater(std::map<std::string, SettingV> &setting, 
                      mshadow::Random<xpu>* prnd) {
    this->prnd_ = prnd;
    SetupUpdater(setting);
  }
  virtual ~AdagradUpdater(void) {}
  
  virtual void Require(std::map<std::string, SettingV> &setting) {
    // default value, just set the value you want
    this->defaults["eps"] = SettingV(0.0001f);
    this->defaults["l2"] = SettingV(0.0f);
    this->defaults["max_iter"] = SettingV(-1);

    // require value, set to SettingV(),
    // it will force custom to set in config
    this->defaults["lr"] = SettingV();
    this->defaults["batch_size"] = SettingV();
    
    Updater<xpu, dim>::Require(setting);
  }
  
  virtual void SetupUpdater(std::map<std::string, SettingV> &setting) {
    Updater<xpu, dim>::SetupUpdater(setting);
	
    this->updater_type = setting["updater_type"].iVal();
    eps = setting["eps"].fVal();
    lr = setting["lr"].fVal();
    max_iter = setting["max_iter"].iVal(); 
    batch_size = setting["batch_size"].iVal(); 
    wd = setting["l2"].fVal(); 
    
    iter = 0;
  }

  struct square_root {
    MSHADOW_XINLINE static real_t Map(real_t a) {
        return sqrt(a);
    }
  };

  virtual void Update(mshadow::Tensor<xpu, dim> data, 
                      mshadow::Tensor<xpu, dim> diff) {

    if (iter == 0) {
      sumGradSquare.Resize(data.shape_, 0.);
    }
    ++iter;
    if (max_iter > 0) {
      iter %= max_iter;
    } 
    
    if (batch_size > 1) {
        diff /= float(batch_size);
    }
                        
    sumGradSquare += diff * diff;
    data -= lr * (diff / (mshadow::expr::F<square_root>(sumGradSquare) + eps));
    if (wd > 0.) {
      data -= (wd*lr) * data;
    }
  }
  
  virtual void UpdateSparse(mshadow::Tensor<xpu, dim> data, 
                            mshadow::Tensor<xpu, dim> diff, 
                            mshadow::Tensor<xpu, 1> idx) {

    if (iter == 0) {
      sumGradSquare.Resize(data.shape_, 0.0f);
    }
    ++iter;
    if (max_iter > 0) {
      iter %= max_iter;
    } 

    if (batch_size > 1) {
        diff /= float(batch_size);
    }

    int w_idx = -1;
    for (int i = 0; i < idx.size(0); ++i) {
      w_idx = idx[i];
      utils::Assert(w_idx >= 0 && w_idx < data.size(0), "");

      mshadow::Tensor<xpu, dim> sumGradSquareRow = sumGradSquare.Slice(w_idx, w_idx+1);
      mshadow::Tensor<xpu, dim> diffRow = diff.Slice(i, i+1);
      mshadow::Tensor<xpu, dim> dataRow = data.Slice(w_idx, w_idx+1);

      sumGradSquareRow += diffRow * diffRow;
      dataRow -= (lr * (diffRow / ((mshadow::expr::F<square_root>(sumGradSquareRow)) + eps)));
      if (wd > 0.) {
        dataRow -= (wd*lr) * dataRow;
      }
    }
  }
 protected: 
  int iter, max_iter, batch_size;
  mshadow::TensorContainer<xpu, dim> sumGradSquare;
  float eps, lr, wd;

};
}  // namespace updater
}  // namespace textnet
#endif  // TEXTNET_CONSTANT_INIT_INL_HPP_
