#include "igl/opengl/glfw/Viewer.h"
#include "igl/readOFF.h"
#include "igl/fit_plane.h"
#include "igl/mat_min.h"
#include "DataSet.hpp"
#include "Mesh.hpp"
#include <Eigen/Geometry>

#include <GLFW/glfw3.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <imgui/imgui.h>

#include <iostream>
#include <fstream>
#include <sstream>

bool showMeshColor = false;
bool showPoints = true;
int viewer_mesh_level = 0;
Mesh<float> mesh;
Eigen::MatrixXd P, P2, current_target, C;
DataSet *ds;

// Splits example point cloud into two point clouds
template <typename T>
void split_point_cloud(const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>& P, Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>& P1, Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>& P2, const Eigen::Matrix<int, Eigen::Dynamic, 1>& id1, Eigen::Matrix<int, Eigen::Dynamic, 1>& id2)
{
  P1.resize(id1.rows(), 3);
  P2.resize(id2.rows(), 3);
  
  for (int i = 0; i < id1.rows(); ++i)
    P1.row(i) = P.row(id1(i));
  
  for (int i = 0; i < id2.rows(); ++i)
    P2.row(i) = P.row(id2(i));
}

template <typename T>
void generate_example_point_cloud(Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>& P, Eigen::Matrix<int, Eigen::Dynamic, 1>& id1, Eigen::Matrix<int, Eigen::Dynamic, 1>& id2)
{
  const int size = 120;
  P.resize(size*size,3);
  id1.resize(size*int(size / 2));
  id2.resize(size*int((size+1) / 2));
  
  int cntr1 = 0, cntr2 = 0;
  for (int x = 0; x < size; ++x)
  {
    const int multipl = x / 30 + 1;
    for (int y = 0; y < size; ++y)
    {
      T height;
      
      if (int(y / (size / (5*multipl))) % 2)
        height = 15;
      else
        height = 0;
        
      Eigen::Matrix<T, 1, 3> row(T(x), height, T(y));
      P.row(x + y*size) = row;
      
      if (x < size / 2)
      {
        id1(cntr1++) = x + y*size;
      }
      else
      {
        id2(cntr2++) = x + y*size;
      }
        
    }
  }
  
}

void reload_viewer_data(igl::opengl::glfw::Viewer &viewer, const Eigen::MatrixXd pc, const Eigen::MatrixXd C, const unsigned int mesh_level, const bool color, const bool showPoints)
{   
    viewer.data().clear();
    
    if (showPoints)
    {
      viewer.data().point_size = 5;
      viewer.data().add_points(pc, C);
    }
    
    Eigen::MatrixXf vertices;
    Eigen::MatrixXi faces;
    ColorData colorData;
    mesh.get_mesh(mesh_level, vertices, faces, colorData);
    viewer.data().set_mesh(vertices.cast<double>(), faces);
      
    if (color)
    {     
      viewer.data().set_texture(colorData.texture.red, colorData.texture.green, colorData.texture.blue);
      viewer.data().set_uv(colorData.UV);
      viewer.data().set_colors(Eigen::RowVector3d(1.f, 1.f, 1.f)); // https://github.com/libigl/libigl/issues/570
      viewer.core.lighting_factor = 0.5;
      viewer.data().show_texture = true;
      viewer.data().show_lines = false;
    }else
    {
      mesh.get_mesh(mesh_level, vertices, faces);
      viewer.data().set_mesh(vertices.cast<double>(), faces);
      viewer.data().show_texture = false;
      viewer.data().show_lines = true;
    }

    viewer.data().compute_normals();
    viewer.data().invert_normals = true;
}

bool callback_key_down(igl::opengl::glfw::Viewer &viewer, unsigned char key, int /*modifier*/)
{
  // Solve one iteration when 1 is pressed
  if (key == '1')
  {
    mesh.solve(1);
    Eigen::MatrixXf vertices;
    Eigen::MatrixXi faces;
    mesh.get_mesh(viewer_mesh_level, vertices, faces);
    viewer.data().set_mesh(vertices.cast<double>(), faces);
    viewer.data().compute_normals();
    viewer.data().invert_normals = true;
  }
  
  if (static_cast<int>(key) == 80)
  {
    showPoints = !showPoints;
    reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints);
  }
  
  if (static_cast<int>(key) == 67)
  {
    showMeshColor = !showMeshColor;
    reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints);
  }
  
  // Move to next point cloud when 2 is pressed
  if (key == '2')
  {
    
    C.resize(1, 3);
    C << 0.f,0.7f,0.7f;

    if (ds) {
      Eigen::Matrix4d t_camera;
      ds->get_next_point_cloud(current_target, C, t_camera);
      
      mesh.set_target_point_cloud(current_target.cast<float>().eval(), C.cast<float>().eval());
    }else
    {
      // If example point cloud switch to second part of it
      current_target = P2;
      mesh.set_target_point_cloud(current_target.cast<float>().eval());
    }
  
    reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints);
  }
  
  if (key == '3' && ds)
  {
    C.resize(1, 3);
    C << 0.f,0.7f,0.7f;
    
    for (int i = 0; i < 10; ++i)
    {
      if (ds) {
        Eigen::Matrix4d t_camera;
        ds->get_next_point_cloud(current_target, C, t_camera);
        
        mesh.set_target_point_cloud(current_target.cast<float>().eval(), C.cast<float>().eval());
      }else
      {
        current_target = P2;
        mesh.set_target_point_cloud(current_target.cast<float>().eval());
      }
      
      mesh.solve(20);
    }
    
      reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints);

  }

  if (key == '4' && ds)
  {
    // Set Viewer to tight draw loop
    viewer.core.is_animating = !viewer.core.is_animating;
  }
  
  // Increase displayed mesh resolution
  if (key == '0')
  {
    viewer_mesh_level = viewer_mesh_level >= MESH_LEVELS - 1 ? viewer_mesh_level : viewer_mesh_level + 1;
    reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints);
  }
  
  // Reduce displayed mesh resolution
  if (key == '-')
  {
    viewer_mesh_level = viewer_mesh_level < 1 ? viewer_mesh_level : viewer_mesh_level - 1;
    reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints); 
  }
  
  
  return true;
}

int main(int argc, char* argv[]) {
    // Read points and normals
    // igl::readOFF(argv[1],P,F,N);
    
    C.resize(1,3);
    C << 0.f,0.7f,0.7f;
        
    if (argc > 2)
    {
      std::cout << "Usage: $0" << std::endl;
      return EXIT_SUCCESS;
    } else if (argc > 1) {
      // Load dataset
      std::string folder(argv[1]);
      ds = new DataSet(folder);
      Eigen::Matrix4d t_camera;
      ds->get_next_point_cloud(P, C, t_camera);
      current_target = P;
      
      mesh.align_to_point_cloud(P.cast<float>().eval());
      mesh.set_target_point_cloud(current_target.cast<float>().eval(), C.cast<float>().eval());
      showMeshColor = true;
    } else {
      // Create example point cloud
      Eigen::VectorXi id1, id2;
      generate_example_point_cloud(P, id1, id2);
      split_point_cloud(P, current_target, P2, id1, id2);
      
      mesh.align_to_point_cloud(P.cast<float>().eval());
      mesh.set_target_point_cloud(current_target.cast<float>().eval());
    }
    
    igl::opengl::glfw::Viewer viewer;    
    viewer.callback_key_down = callback_key_down;
    
    viewer.callback_pre_draw = [&](igl::opengl::glfw::Viewer & v)->bool 
    { 
      if (!ds || !viewer.core.is_animating) return false;

      Eigen::Matrix4d t_camera;
      bool cont = ds->get_next_point_cloud(P, C, t_camera);
      if (!cont)
      {
        // end of image sequence 
        viewer.core.is_animating = false;
        return false;
      }
      
      mesh.set_target_point_cloud(P.cast<float>().eval(), C.cast<float>().eval());
      mesh.solve(1);
      reload_viewer_data(viewer, P, C, viewer_mesh_level, showMeshColor, showPoints);
      
      return false; 
    }; 

    reload_viewer_data(viewer, current_target, C, viewer_mesh_level, showMeshColor, showPoints);
    viewer.core.align_camera_center(current_target);
    
    igl::opengl::glfw::imgui::ImGuiMenu menu;
    viewer.plugins.push_back(&menu);
    
    viewer.core.background_color = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

    menu.callback_draw_viewer_menu = [&]()
    {
      ImGui::Text("Iterate: \"1\"");
      ImGui::Text("Next point cloud: \"2\"");
      ImGui::Text("Lots of iterations and point clouds (dataset only): \"3\"");
      ImGui::Text("Play image sequence (%s): \"4\"", viewer.core.is_animating ? "on" : "off");
      ImGui::Text("Change mesh level (%d): 0/+", viewer_mesh_level);
      ImGui::Text("Toggle coloring (%s): C", showMeshColor ? "on" : "off");
      ImGui::Text("Toggle point cloud (%s): P", showPoints ? "on" : "off");
    };
    
    viewer.launch();

    // cleanup
    mesh.cleanup();
}
