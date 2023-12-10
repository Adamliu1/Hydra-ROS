/* -----------------------------------------------------------------------------
 * Copyright 2022 Massachusetts Institute of Technology.
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Research was sponsored by the United States Air Force Research Laboratory and
 * the United States Air Force Artificial Intelligence Accelerator and was
 * accomplished under Cooperative Agreement Number FA8750-19-2-1000. The views
 * and conclusions contained in this document are those of the authors and should
 * not be interpreted as representing the official policies, either expressed or
 * implied, of the United States Air Force or the U.S. Government. The U.S.
 * Government is authorized to reproduce and distribute reprints for Government
 * purposes notwithstanding any copyright notation herein.
 * -------------------------------------------------------------------------- */
#pragma once
#include <hydra/frontend/frontend_module.h>
#include <hydra/frontend/mesh_segmenter.h>
#include <hydra_msgs/ActiveLayer.h>
#include <hydra_msgs/ActiveMesh.h>
#include <kimera_pgmo/MeshFrontend.h>
#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>
#include <ros/ros.h>
#include <tf2_ros/transform_listener.h>

#include "hydra_ros/utils/dsg_streaming_interface.h"
#include "hydra_ros/utils/semantic_ros_publishers.h"

namespace hydra {

using hydra_msgs::ActiveLayer;
using hydra_msgs::ActiveMesh;
using pose_graph_tools_msgs::PoseGraph;

using ObjectCloudPub = SemanticRosPublishers<uint8_t, MeshSegmenter::MeshVertexCloud>;
using MeshVertexCloud = MeshSegmenter::MeshVertexCloud;
using LabelIndices = MeshSegmenter::LabelIndices;

struct RosFrontendConfig : public FrontendConfig {
  std::string frontend_ns = "~";
  bool enable_active_mesh_pub = false;
  bool enable_segmented_mesh_pub = false;
  std::string odom_frame = "odom";
  std::string sensor_frame = "left_cam";
  bool use_latest_tf = true;
  bool use_posegraph_pos = true;
};

class RosFrontend : public FrontendModule {
 public:
  using Policy =
      message_filters::sync_policies::ApproximateTime<ActiveLayer, ActiveMesh>;
  using Sync = message_filters::Synchronizer<Policy>;

  RosFrontend(const RosFrontendConfig& config,
              const SharedDsgInfo::Ptr& dsg,
              const SharedModuleState::Ptr& state,
              const LogSetup::Ptr& log_setup);

  ~RosFrontend();

  std::string printInfo() const override;

 protected:
  void inputCallback(const ActiveLayer::ConstPtr& places,
                     const ActiveMesh::ConstPtr& mesh);

  void poseGraphCallback(const PoseGraph::ConstPtr& pose_graph);

  void publishActiveVertices(const MeshSegmenter::MeshVertexCloud& vertices,
                             const MeshSegmenter::IndicesVector& indices,
                             const MeshSegmenter::LabelIndices&) const;

  void publishObjectClouds(const MeshSegmenter::MeshVertexCloud& vertices,
                           const MeshSegmenter::IndicesVector&,
                           const MeshSegmenter::LabelIndices& label_indices) const;

  std::optional<Eigen::Vector3d> getLatestPosition() const;

  std::optional<Eigen::Vector3d> getLatestPositionTf(
      const ros::Time& time_to_use) const;

 protected:
  const RosFrontendConfig config_;
  ros::NodeHandle nh_;
  std::list<PoseGraph::ConstPtr> pose_graph_queue_;

  std::unique_ptr<message_filters::Subscriber<ActiveLayer>> places_sub_;
  std::unique_ptr<message_filters::Subscriber<ActiveMesh>> mesh_sub_;
  std::unique_ptr<Sync> sync_;

  ros::Subscriber pose_graph_sub_;

  tf2_ros::Buffer buffer_;
  std::unique_ptr<tf2_ros::TransformListener> tf_listener_;

  ros::Publisher active_vertices_pub_;
  std::unique_ptr<ObjectCloudPub> segmented_vertices_pub_;

  inline static const auto registration_ =
      config::RegistrationWithConfig<FrontendModule,
                                     RosFrontend,
                                     RosFrontendConfig,
                                     SharedDsgInfo::Ptr,
                                     SharedModuleState::Ptr,
                                     LogSetup::Ptr>("RosFrontend");
};

}  // namespace hydra
