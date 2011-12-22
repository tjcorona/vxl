#include "boxm2_multi_store_aux.h"
#include <boxm2_multi_util.h>

#include <vcl_algorithm.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_util.h>
#include <bocl/bocl_manager.h>
#include <boxm2/ocl/boxm2_ocl_util.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/ocl/algo/boxm2_ocl_camera_converter.h>

#include <bocl/bocl_mem.h>
#include <bocl/bocl_device.h>
#include <bocl/bocl_kernel.h>
#include <brdb/brdb_value.h>
#include <brdb/brdb_selection.h>
#include <bprb/bprb_batch_process_manager.h>
#include <bprb/bprb_parameters.h>
#include <bprb/bprb_macros.h>
#include <bprb/bprb_func_process.h>
#include <vil/vil_image_view_base.h>
#include <vil/vil_save.h>
#include <vpgl/vpgl_perspective_camera.h>
#include <vnl/vnl_random.h>
#include <vul/vul_timer.h>


vcl_map<vcl_string, bocl_kernel*> boxm2_multi_store_aux::kernels_;


//-------------------------------------------------------------
// Stores seg len and observation in cell-level aux data
//-------------------------------------------------------------
float boxm2_multi_store_aux::store_aux(       boxm2_multi_cache&         cache, 
                                      const vil_image_view<float>&   img, 
                                            vpgl_camera_double_sptr  cam )
{
  vcl_cout<<"------------ boxm2_multi_store_aux store aux--------------"<<vcl_endl;
  //verify appearance model
  vcl_size_t lthreads[2] = {8,8};
  vcl_string data_type, options;
  int apptypesize; 
  if( !boxm2_multi_util::get_scene_appearances(cache.get_scene(), data_type, options, apptypesize) ) 
    return 0.0f; 
   
  //setup image size
  int ni=img.ni(), 
      nj=img.nj();

  //set up image lists
  vcl_vector<cl_command_queue> queues; 
  
  //for each device/cache, run an update
  vcl_vector<bocl_mem_sptr> out_imgs; 
  vcl_vector<boxm2_opencl_cache*>  ocl_caches = cache.get_vis_sub_scenes( (vpgl_perspective_camera<double>*) cam.ptr());
  for(int i=0; i<ocl_caches.size(); ++i) {
    
    //grab sub scene and it's cache
    boxm2_opencl_cache*     ocl_cache = ocl_caches[i]; 
    boxm2_scene_sptr        sub_scene = ocl_cache->get_scene(); 
    bocl_device_sptr        device    = ocl_cache->get_device(); 
    
    // compile the kernel/retrieve cached kernel for this device
    bocl_kernel* kern = get_kernels(device, options);

    // create a command queue.
    int status=0;
    cl_command_queue queue = clCreateCommandQueue( device->context(),
                                                   *(device->device_id()),
                                                   CL_QUEUE_PROFILING_ENABLE,
                                                   &status );
    queues.push_back(queue); 
    if (status!=0) {
      vcl_cout<<"boxm2_multi_store_aux::store_aux unable to create command queue"<<vcl_endl;
      return 0.0f;
    }

    //prepare kernel variables
    //create image and workspace size
    unsigned cl_ni=RoundUp(ni,lthreads[0]);
    unsigned cl_nj=RoundUp(nj,lthreads[1]);
    float* inImg = new float[cl_ni*cl_nj];
    int c = 0; 
    for(int j=0; j<nj; ++j) 
      for(int i=0; i<ni; ++i)
        inImg[c++] = img(i,j); 
    bocl_mem_sptr in_mem = new bocl_mem(device->context(),inImg,cl_ni*cl_nj*sizeof(float),"exp image buffer");
    in_mem->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

    //create image dim buff
    int img_dim_buff[4] = {0,0,ni,nj}; 
    bocl_mem_sptr img_dim = new bocl_mem(device->context(), img_dim_buff, sizeof(int)*4, "image dims");
    img_dim->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    
    // Output Array
    float* output_arr = new float[cl_ni*cl_nj];
    vcl_fill(output_arr, output_arr+cl_ni*cl_nj, 0.0f);
    bocl_mem_sptr  cl_output=new bocl_mem(device->context(), output_arr, sizeof(float)*cl_ni*cl_nj, "output buffer");
    cl_output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    out_imgs.push_back(cl_output);

    //store aux data
    store_aux_scene( sub_scene, device, ocl_cache, queue, 
                     cam, in_mem, img_dim, data_type, 
                     kern, lthreads, cl_ni, cl_nj, apptypesize, cl_output); 
                    
    delete[] inImg; 
    //delete[] output_arr;
  }

//==== DEBUG ====
#if 1
  unsigned cl_ni=RoundUp(ni,lthreads[0]);
  unsigned cl_nj=RoundUp(nj,lthreads[1]);
  vil_image_view<float> segLens(cl_ni,cl_nj);
  segLens.fill(0.0f);
  for(int i=0; i<queues.size(); ++i) {
    clFinish(queues[i]); 
    out_imgs[i]->read_to_buffer(queues[i]); 
    float* output_arr = (float*) out_imgs[i]->cpu_buffer();
    int count=0;
    for(int j=0; j<cl_nj; ++j)
      for(int i=0; i<cl_ni; ++i)
        segLens(i,j) += output_arr[count++];
    delete[] output_arr; 
  }
  vil_save(segLens, "seg_len.tiff");
#endif
//==============

  //-------------------------------------
  //finish execution along each queue (block c++ until all GPUS are done
  //-------------------------------------
  for(int i=0; i<queues.size(); ++i) {
    clFinish(queues[i]); 
    clReleaseCommandQueue(queues[i]); 
  }
}

//MAP function for store aux scene step
float boxm2_multi_store_aux::store_aux_scene(boxm2_scene_sptr          scene,
                                        bocl_device_sptr          device,
                                        boxm2_opencl_cache*       opencl_cache,
                                        cl_command_queue &        queue,
                                        vpgl_camera_double_sptr & cam,
                                        bocl_mem_sptr &           in_image,
                                        bocl_mem_sptr &           img_dim,
                                        vcl_string                data_type,
                                        bocl_kernel*              kernel,
                                        vcl_size_t *              lthreads,
                                        unsigned                  cl_ni,
                                        unsigned                  cl_nj,
                                        int                       apptypesize,
                                        bocl_mem_sptr &           cl_output )
{
  //camera check
  if (cam->type_name()!= "vpgl_perspective_camera" &&
      cam->type_name()!= "vpgl_generic_camera" ) {
    vcl_cout<<"Cannot render with camera of type "<<cam->type_name()<<vcl_endl;
    return 0.0f;
  }

  //set generic cam and get visible block order
  cl_float* ray_origins    = new cl_float[4*cl_ni*cl_nj];
  cl_float* ray_directions = new cl_float[4*cl_ni*cl_nj];
  bocl_mem_sptr ray_o_buff = new bocl_mem(device->context(), ray_origins   ,  cl_ni*cl_nj * sizeof(cl_float4), "ray_origins buffer");
  bocl_mem_sptr ray_d_buff = new bocl_mem(device->context(), ray_directions,  cl_ni*cl_nj * sizeof(cl_float4), "ray_directions buffer");
  boxm2_ocl_camera_converter::compute_ray_image( device, queue, cam, cl_ni, cl_nj, ray_o_buff, ray_d_buff);

  // bit lookup buffer
  cl_uchar lookup_arr[256];
  boxm2_ocl_util::set_bit_lookup(lookup_arr);
  bocl_mem_sptr lookup=new bocl_mem(device->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
  lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //2. set global thread size
  vcl_size_t gThreads[] = {cl_ni,cl_nj};

  // set arguments
  vcl_vector<boxm2_block_id> vis_order = scene->get_vis_blocks(cam);
  vcl_vector<boxm2_block_id>::iterator id;
  for (id = vis_order.begin(); id != vis_order.end(); ++id)
  {
    //vcl_cout<<(*id);
    //choose correct render kernel
    boxm2_block_metadata mdata = scene->get_block_metadata(*id);
    bocl_kernel* kern =  kernel;

    //write the image values to the buffer
    bocl_mem* blk       = opencl_cache->get_block(*id);
    bocl_mem* blk_info  = opencl_cache->loaded_block_info();
    bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(*id,0,false);
    
    //calc data buffer length
    boxm2_scene_info* info_buffer = (boxm2_scene_info*) blk_info->cpu_buffer();
    int alphaTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_ALPHA>::prefix());
    info_buffer->data_buffer_length = (int) (alpha->num_bytes()/alphaTypeSize);
    blk_info->write_to_buffer((queue));
    int nobsTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_NUM_OBS>::prefix());
    
    //grab an appropriately sized AUX data buffer
    int auxTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX0>::prefix());
    bocl_mem *aux0   = opencl_cache->get_data<BOXM2_AUX0>(*id, info_buffer->data_buffer_length*auxTypeSize);
    auxTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX1>::prefix());
    bocl_mem *aux1   = opencl_cache->get_data<BOXM2_AUX1>(*id, info_buffer->data_buffer_length*auxTypeSize);
    aux0->zero_gpu_buffer(queue);
    aux1->zero_gpu_buffer(queue);
    
    //set args
    kern->set_arg( blk_info );
    kern->set_arg( blk );
    kern->set_arg( alpha );
    kern->set_arg( aux0 );
    kern->set_arg( aux1 );
    kern->set_arg( lookup.ptr() );

    // kern->set_arg( persp_cam.ptr() );
    kern->set_arg( ray_o_buff.ptr() );
    kern->set_arg( ray_d_buff.ptr() );

    kern->set_arg( img_dim.ptr() );
    kern->set_arg( in_image.ptr() );
    kern->set_arg( cl_output.ptr() );
    kern->set_local_arg( lthreads[0]*lthreads[1]*sizeof(cl_uchar16) );//local tree,
    kern->set_local_arg( lthreads[0]*lthreads[1]*sizeof(cl_uchar4) ); //ray bundle,
    kern->set_local_arg( lthreads[0]*lthreads[1]*sizeof(cl_int) );    //cell pointers,
    kern->set_local_arg( lthreads[0]*lthreads[1]*sizeof(cl_float4) ); //cached aux,
    kern->set_local_arg( lthreads[0]*lthreads[1]*10*sizeof(cl_uchar) ); //cumsum buffer, imindex buffer

    //execute kernel
    kern->execute(queue, 2, lthreads, gThreads);
    //int status = clFinish(queue);
    //check_val(status, MEM_FAILURE, "UPDATE EXECUTE FAILED: " + error_to_string(status));
    //gpu_time += kern->exec_time();

    //clear render kernel args so it can reset em on next execution
    kern->clear_args();

    //aux0->read_to_buffer(queue);
    //aux1->read_to_buffer(queue);
  }

  //clean up cam
  delete[] ray_origins;
  delete[] ray_directions;
  return 0.0f;
}



//-----------------------------------------------------------------
// returns vector of bocl_kernels for this specific device
//-----------------------------------------------------------------
bocl_kernel* boxm2_multi_store_aux::get_kernels(bocl_device_sptr device, vcl_string opts)
{
  // check to see if this device has compiled kernels already
  vcl_string identifier = device->device_identifier()+opts;
  if (kernels_.find(identifier) != kernels_.end()) 
    return kernels_[identifier]; 

  //if not, compile and cache them
  vcl_cout<<"===========Compiling multi update kernels===========\n"
          <<"  for device: "<<device->device_identifier()<<vcl_endl;

  //gather all render sources... seems like a lot for rendering...
  vcl_vector<vcl_string> src_paths;
  vcl_string source_dir = boxm2_ocl_util::ocl_src_root();
  src_paths.push_back(source_dir + "scene_info.cl");
  src_paths.push_back(source_dir + "cell_utils.cl");
  src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
  src_paths.push_back(source_dir + "backproject.cl");
  src_paths.push_back(source_dir + "statistics_library_functions.cl");
  src_paths.push_back(source_dir + "ray_bundle_library_opt.cl");
  src_paths.push_back(source_dir + "bit/update_kernels.cl");
  src_paths.push_back(source_dir + "update_functors.cl");
  src_paths.push_back(source_dir + "bit/cast_ray_bit.cl");
  
  //compilation options
  vcl_string options = opts+" -D INTENSITY  ";
  options += " -D DETERMINISTIC ";

  //create all passes
  bocl_kernel* seg_len = new bocl_kernel();
  vcl_string seg_opts = options + " -D SEGLEN -D STEP_CELL=step_cell_seglen(aux_args,data_ptr,llid,d) ";
  seg_len->create_kernel(&device->context(),device->device_id(), src_paths, "seg_len_main", seg_opts, "update::seg_len");

  //cache in map
  kernels_[identifier] = seg_len; 
  return kernels_[identifier]; 
}
