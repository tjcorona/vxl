#ifndef betr_algorithm_h_
#define betr_algorithm_h_
//:
// \file
// \brief The base class for betr algorithms
// \author J.L. Mundy
// \date July 4, 2016
//
#include <string>
#include <vcl_compiler.h>
#include <vbl/vbl_ref_count.h>
#include <vil/vil_image_resource.h>
#include <vsol/vsol_box_2d.h>
#include <vsol/vsol_polygon_2d.h>
#include <vsol/vsol_polygon_2d_sptr.h>
#include "betr_params.h"
class betr_algorithm : public vbl_ref_count
{
 public:
  betr_algorithm():name_("no_name"), identifier_("null"), offset_(0.0), alpha_(1.0), verbose_(false), params_(VXL_NULLPTR){}
 betr_algorithm(std::string const& name): name_(name),offset_(0.0), alpha_(1.0), verbose_(false), params_(VXL_NULLPTR){}
 betr_algorithm(std::string const& name, double offset, double alpha): name_(name),offset_(offset), alpha_(alpha), verbose_(false), params_(VXL_NULLPTR), multiple_ref_(false){}
 betr_algorithm(std::string const& name,betr_params_sptr const& params, double offset, double alpha): name_(name),offset_(offset), alpha_(alpha), verbose_(false), params_(params), multiple_ref_(false){}
  
  // performance parameters
  void set_offset(double offset){offset_ = offset;}
  void set_alpha(double alpha){alpha_ = alpha;}
  void set_params(betr_params_sptr const& params){params_ = params;}
  //: data inputs
  void set_reference_image(vil_image_resource_sptr const& ref_imgr){ref_rescs_.clear(); ref_rescs_.push_back(ref_imgr);}
  void set_reference_images(std::vector<vil_image_resource_sptr> const& ref_rescs){ref_rescs_ = ref_rescs;}
  void set_event_image(vil_image_resource_sptr const& evt_imgr){evt_imgr_ = evt_imgr;}
  void set_proj_ref_ref_object(vsol_polygon_2d_sptr const& ref_poly)
  {ref_ref_polys_.clear();ref_ref_polys_.push_back(ref_poly);}
  void set_proj_ref_ref_objects(std::vector<vsol_polygon_2d_sptr> const& ref_polys){ref_ref_polys_= ref_polys;}
  void set_proj_ref_evt_object(vsol_polygon_2d_sptr const& evt_poly)
  {ref_evt_polys_.clear();ref_evt_polys_.push_back(evt_poly);}
  void set_proj_ref_evt_objects(std::vector<vsol_polygon_2d_sptr> const& evt_polys){ref_evt_polys_= evt_polys;}
  void set_proj_evt_ref_object(vsol_polygon_2d_sptr const& ref_poly){evt_ref_poly_ = ref_poly;}
  void set_proj_evt_evt_object(vsol_polygon_2d_sptr const& evt_poly){evt_evt_poly_ = evt_poly;}
  //: accessors
  std::string name() const {return name_;}
  betr_params_sptr params(){return params_;}
  bool requires_multiple_ref_images() const{return multiple_ref_;}
  //: procedural  methods
  virtual bool process(){return false;}
  virtual double prob_change() const {return 0.0;}
  //: offset is with respect to the event image coordinate system
  virtual vil_image_resource_sptr change_image(unsigned& i_offset, unsigned& j_offset) const {return VXL_NULLPTR;}
  virtual void clear(){
    ref_rescs_.clear();
    evt_imgr_ = VXL_NULLPTR;
    ref_ref_polys_.clear();
    ref_evt_polys_.clear();
    evt_ref_poly_ = VXL_NULLPTR;
    evt_evt_poly_ = VXL_NULLPTR;
  }
    //: debug 
  //======================
  void set_verbose(bool verbose){verbose_ = verbose;}
  //: an identifier for a particular execution run
  void set_identifier(std::string identifier){identifier_ = identifier;}

 protected:
  std::string name_;//algorithm name
  std::string identifier_;
  std::vector<vil_image_resource_sptr> ref_rescs_;
  vil_image_resource_sptr evt_imgr_;
  //  vsol_polygon_2d_sptr ref_ref_poly_;
  // vsol_polygon_2d_sptr ref_evt_poly_;
  std::vector< vsol_polygon_2d_sptr> ref_ref_polys_;
  std::vector< vsol_polygon_2d_sptr> ref_evt_polys_;
  vsol_polygon_2d_sptr evt_ref_poly_;
  vsol_polygon_2d_sptr evt_evt_poly_;
  //as in p_change = 1/(1+e^-alpha*(change-offset))
  double offset_;
  double alpha_;
  bool verbose_;
  bool multiple_ref_;//does the algorithm require multiple reference images
  betr_params_sptr params_;
};
#endif   // DO NOT ADD CODE AFTER THIS LINE! END OF DEFINITION FOR CLASS betr_algorithm.
#include "betr_algorithm_sptr.h"
