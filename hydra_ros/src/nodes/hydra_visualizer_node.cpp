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
#include <config_utilities/config.h>
#include <config_utilities/parsing/ros.h>
#include <config_utilities/printing.h>
#include <glog/logging.h>
#include <hydra/utils/timing_utilities.h>
#include <ros/ros.h>
#include <std_srvs/Empty.h>

#include <fstream>

#include "hydra_ros/utils/dsg_streaming_interface.h"
#include "hydra_ros/visualizer/dynamic_scene_graph_visualizer.h"
#include "hydra_ros/visualizer/mesh_plugin.h"

using DsgVisualizer = hydra::DynamicSceneGraphVisualizer;
using spark_dsg::getDefaultLayerIds;

namespace hydra {

struct NodeConfig {
  bool load_graph = false;
  std::string scene_graph_filepath = "";
  std::string visualizer_ns = "/hydra_dsg_visualizer";
  std::string mesh_plugin_ns = "dsg_mesh";
  std::string output_path = "";
};

void declare_config(NodeConfig& config) {
  using namespace config;
  name("VisualizerNodeConfig");
  field(config.load_graph, "load_graph");
  field(config.scene_graph_filepath, "scene_graph_filepath");
  field(config.visualizer_ns, "visualizer_ns");
  field(config.mesh_plugin_ns, "mesh_plugin_ns");
  field(config.output_path, "output_path");
}

struct VisualizerNode {
  VisualizerNode(const ros::NodeHandle& nh) : nh_(nh) {
    config_ = config::fromRos<hydra::NodeConfig>(nh);
    ROS_INFO_STREAM("Config: " << std::endl << config_);

    ros::NodeHandle viz_nh(config_.visualizer_ns);
    visualizer_.reset(new DsgVisualizer(viz_nh, getDefaultLayerIds()));
    visualizer_->addPlugin(
        std::make_shared<MeshPlugin>(viz_nh, config_.mesh_plugin_ns));

    if (!config_.output_path.empty()) {
      size_log_file_.reset(
          new std::ofstream(config_.output_path + "/dsg_message_sizes.csv"));
      *size_log_file_ << "time_ns,bytes" << std::endl;
    }

    if (!config_.load_graph || config_.scene_graph_filepath.empty()) {
      receiver_.reset(new DsgReceiver(nh_, [&](const ros::Time& stamp, size_t bytes) {
        if (size_log_file_) {
          *size_log_file_ << stamp.toNSec() << "," << bytes << std::endl;
        }
      }));
    } else {
      loadGraph();
    }
  }

  ~VisualizerNode() {
    std::cout << "timing stats: "
              << hydra::timing::ElapsedTimeRecorder::instance().getStats("receive_dsg")
              << std::endl;
    std::cout << "mesh timing stats: "
              << hydra::timing::ElapsedTimeRecorder::instance().getStats("receive_mesh")
              << std::endl;
  }

  void loadGraph() {
    ROS_INFO_STREAM("Loading dsg from: " << config_.scene_graph_filepath);
    auto dsg = hydra::DynamicSceneGraph::load(config_.scene_graph_filepath);
    ROS_INFO_STREAM("Loaded dsg: " << dsg->numNodes() << " nodes, " << dsg->numEdges()
                                   << " edges, has mesh? "
                                   << (dsg->hasMesh() ? "yes" : "no"));
    visualizer_->setGraph(dsg);
  }

  bool handleService(std_srvs::Empty::Request&, std_srvs::Empty::Response&) {
    loadGraph();
    return true;
  }

  void spin() {
    ROS_DEBUG("Visualizer running");

    if (!config_.load_graph) {
      bool graph_set = false;

      ros::Rate r(10);
      while (ros::ok()) {
        ros::spinOnce();

        if (receiver_ && receiver_->updated()) {
          if (!receiver_->graph()) {
            r.sleep();
            continue;
          }
          if (!graph_set) {
            visualizer_->setGraph(receiver_->graph());
            graph_set = true;
          } else {
            visualizer_->setGraphUpdated();
          }

          visualizer_->redraw();
          receiver_->clearUpdated();
        }

        r.sleep();
      }
    } else {
      reload_service_ =
          nh_.advertiseService("reload", &VisualizerNode::handleService, this);
      visualizer_->start();
      ros::spin();
    }
  }

  ros::NodeHandle nh_;
  std::unique_ptr<DsgVisualizer> visualizer_;
  std::unique_ptr<DsgReceiver> receiver_;
  NodeConfig config_;
  std::unique_ptr<std::ofstream> size_log_file_;
  ros::ServiceServer reload_service_;
};

}  // namespace hydra

int main(int argc, char** argv) {
  ros::init(argc, argv, "dsg_visualizer_node");

  FLAGS_minloglevel = 3;
  FLAGS_logtostderr = 1;
  FLAGS_colorlogtostderr = 1;

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  ros::NodeHandle nh("~");
  hydra::VisualizerNode node(nh);
  node.spin();

  return 0;
}
