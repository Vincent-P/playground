#pragma once
/**

   Z prepass:
   depth output:
       "depth buffer"

   Voxelization:
   storage output:
       "voxels albedo"
       "voxels normal"

   Voxel direct lighting:
   texture sampled input:
       "voxels albedo"
       "voxels normal"
   storage output:
       "voxels radiance"

   Voxel aniso mipmaps:
   texture sampled input:
       "voxels radiance"
   storage output:
       "voxels aniso base"

   Voxel directional volumes:
   texture input:
       "voxels aniso base"
   storage output:
       "voxels directional volumes"

   Draw floor:
   color attachment:
       "hdr buffer"
   depth output:
       "depth buffer"

   Draw glTF
   texture sampled input:
       "voxels radiance"
       "voxels directional volumes"
   color attachment:
       "hdr buffer"
   depth output:
       "depth buffer"

   Visualize voxels
   texture sampled input:
       "voxels albedo"
       "voxels normal"
       "voxels radiance"
       "voxels directional volumes"
   color attachment:
       "hdr buffer"
   depth output:
       "depth buffer"

   Render Transmittance LUT
   color attachment:
       "Transmittance LUT"

   Render MultiScattering LUT
   texture sampled input:
       "Transmittance LUT"
   color attachment:
       "MultiScattering LUT"

   Render SkyView LUT
   texture sampled input:
       "Transmittance LUT"
       "MultiScattering LUT"
   color attachment:
       "SkyView LUT"

   Render Sky
   texture sampled input:
       "Transmittance LUT"
       "MultiScattering LUT"
       "SkyView LUT"
   color attachment:
       "hdr buffer"

   Tonemapping
   texture sampled input:
       "hdr buffer"
   color attachment:
       "swapchain image"

   ImGui
   texture sampled input:
       "imgui atlas"
   color attachment:
       "swapchain image"

 **/
