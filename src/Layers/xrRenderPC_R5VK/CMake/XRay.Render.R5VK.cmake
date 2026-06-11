add_module(XRay.Render.R5VK.ForceIncludes INTERFACE)

target_compile_options(XRay.Render.R5VK.ForceIncludes
  INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/FIr5vk.h>
  $<$<CXX_COMPILER_ID:Clang>:-includer5vk.h>
  $<$<CXX_COMPILER_ID:GNU>:-includer5vk.h>
)

add_module(XRay.Render.R5VK
  TYPE STATIC

  INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../xrRenderPC_R4
  ${CMAKE_CURRENT_SOURCE_DIR}/../xrRenderVK

  DEFINES
  RENDER=5
  STATIC_RENDERER_R5VK
  USE_DX11
  USE_VK
  XRRENDER_R5VK_EXPORTS

  LINKS
  dxgi
  dxsdk
  d3d11
  fastdelegate
  FastDynamicCast
  imgui
  loki
  luabind
  LuaJIT
  NVAPI
  optick
  ReShadeCompat
  robin_hood
  tbb

  Vulkan::Vulkan
  VulkanMemoryAllocator
  DXC

  XRay.Platform
  XRay.Render.R5VK.ForceIncludes

  XRay.Core.Defines
  XRay.Engine.Defines
  XRay.Render.Common.Defines

  XRay.Includes
  XRay.Collision.Includes
  XRay.Core.Includes
  XRay.CPUPipe.Includes
  XRay.Engine.Includes
  XRay.Particles.Includes
  XRay.Physics.Includes
  XRay.Render.API.Includes
  XRay.Render.Common.Includes
  XRay.Render.DX10.Includes
  XRay.ServerEntities.Includes
  XRay.Sound.Includes

  PRECOMPILES
  #[["xrD3DDefs.h"]]
  #[["dx10EventWrapper.h"]]
  #[["psystem.h"]]
  #[["HW.h"]]
  #[["Shader.h"]]
  #[["R_Backend.h"]]
  #[["R_Backend_Runtime.h"]]
  #[["resourcemanager.h"]]
  #[["vis_common.h"]]
  #[["render.h"]]
  #[["_d3d_extensions.h"]]
  #[["igame_level.h"]]
  #[["blenders/blender.h"]]
  #[["blenders/blender_clsid.h"]]
  #[["xrRender_console.h"]]
  #[["r5vk.h"]]

  SOURCES
  ../xrRenderDX10/DXCommonTypes.h
  ../xrRender/xrD3DDefs.h

  ../xrRender/xrRender_console.cpp
  ../xrRender/xrRender_console.h

  ../xrRenderPC_R4/jitter.h

  xrRender_R5VK.cpp
)

add_module(XRay.Render.R5VK.3DFluid
  SOURCES
  ../xrRenderDX10/3DFluid/dx103DFluidData.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidData.h

  ../xrRenderDX10/3DFluid/dx103DFluidEmitters.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidEmitters.h

  ../xrRenderDX10/3DFluid/dx103DFluidGrid.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidGrid.h

  ../xrRenderDX10/3DFluid/dx103DFluidManager.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidManager.h

  ../xrRenderDX10/3DFluid/dx103DFluidObstacles.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidObstacles.h

  ../xrRenderDX10/3DFluid/dx103DFluidRenderer.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidRenderer.h
)

add_module(XRay.Render.R5VK.Core
  SOURCES
  ../xrRenderDX10/dx10Texture.cpp
  ../xrRender/particles_systems_library_interface.hpp
  ../xrRender/PSLibrary.cpp
  ../xrRender/PSLibrary.h
  ../xrRender/QueryHelper.h
  ../xrRender/r__dsgraph_build.cpp
  ../xrRender/r__dsgraph_render.cpp
  ../xrRender/r__dsgraph_render_lods.cpp
  ../xrRender/r__dsgraph_structure.h
  ../xrRender/r__dsgraph_types.h
  ../xrRender/r__occlusion.cpp
  ../xrRender/r__occlusion.h
  ../xrRender/r__pixel_calculator.cpp
  ../xrRender/r__pixel_calculator.h
  ../xrRender/r__screenshot.cpp
  ../xrRender/r_sun_cascades.h
  ../xrRenderPC_R4/r2_blenders.cpp
  ../xrRenderPC_R4/r2_R_calculate.cpp
  ../xrRenderPC_R4/r2_R_lights.cpp
  ../xrRenderPC_R4/r2_R_sun.cpp
  ../xrRenderPC_R4/r2_sector_detect.cpp
  r2_test_hw_vk.cpp
  ../xrRenderPC_R4/r2_types.h
  ../xrRenderPC_R4/r4.cpp
  ../xrRenderPC_R4/r4.h
  r5vk.h
  ../xrRenderPC_R4/r4_loader.cpp
  ../xrRenderPC_R4/r4_R_rain.cpp
  ../xrRenderPC_R4/r4_R_render.cpp
  ../xrRenderPC_R4/r4_R_sun_support.cpp
  ../xrRenderPC_R4/r4_R_sun_support.h
  ../xrRender/tga.cpp
  ../xrRender/tga.h
)

add_module(XRay.Render.R5VK.Core.Target
  SOURCES
  ../xrRender/rendertarget_phase_blur.cpp
  ../xrRender/rendertarget_phase_dof.cpp
  ../xrRender/rendertarget_phase_lut.cpp
  ../xrRender/rendertarget_phase_nightvision.cpp
  ../xrRender/rendertarget_phase_gasmask_drops.cpp
  ../xrRender/rendertarget_phase_gasmask_dudv.cpp
  ../xrRender/rendertarget_phase_pp_bloom.cpp
  ../xrRender/rendertarget_phase_smaa.cpp
  ../xrRender/rendertarget_phase_sunshafts.cpp
  ../xrRenderPC_R4/r4_rendertarget.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_direct.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_omnipart_geom.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_point.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_point_geom.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_reflected.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_spot.cpp
  ../xrRenderPC_R4/r4_rendertarget_accum_spot_geom.cpp
  ../xrRenderPC_R4/r4_rendertarget_create_minmaxSM.cpp
  ../xrRenderPC_R4/r4_rendertarget_draw_rain.cpp
  ../xrRenderPC_R4/r4_rendertarget_draw_volume.cpp
  ../xrRenderPC_R4/r4_rendertarget_enable_scissor.cpp
  ../xrRenderPC_R4/r4_rendertarget_mark_msaa_edges.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_bloom.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_combine.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_hdao.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_hdr10_bloom.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_hdr10_lens_flare.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_luminance.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_occq.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_PP.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_rain.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_scene.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_smap_D.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_smap_S.cpp
  ../xrRenderPC_R4/r4_rendertarget_phase_ssao.cpp
  ../xrRenderPC_R4/r4_rendertarget.h
  ../xrRenderPC_R4/r4_rendertarget_wallmarks.h
)

add_module(XRay.Render.R5VK.Core.Target.ColorMap
  SOURCES
  ../xrRender/ColorMapManager.cpp
  ../xrRender/ColorMapManager.h
)

add_module(XRay.Render.R5VK.Debug
  SOURCES
  ../xrRenderDX10/dx10EventWrapper.cpp
  ../xrRenderDX10/dx10EventWrapper.h
)

add_module(XRay.Render.R5VK.Details
  SOURCES
  ../xrRender/DetailFormat.h
  ../xrRender/DetailManager.cpp
  ../xrRender/DetailManager.h
  ../xrRender/DetailManager_CACHE.cpp
  ../xrRender/DetailManager_Decompress.cpp
  ../xrRender/DetailManager_soft.cpp
  ../xrRender/DetailManager_VS.cpp
  ../xrRender/DetailModel.cpp
  ../xrRender/DetailModel.h
  ../xrRenderDX10/dx10DetailManager_VS.cpp
)

add_module(XRay.Render.R5VK.DX9ToDX10Utils
  SOURCES
  ../xrRenderDX10/dx10BufferUtils.cpp
  ../xrRenderDX10/dx10BufferUtils.h

  ../xrRenderDX10/dx10StateUtils.cpp
  ../xrRenderDX10/dx10StateUtils.h

  ../xrRenderDX10/dx10TextureUtils.cpp
  ../xrRenderDX10/dx10TextureUtils.h
)

add_module(XRay.Render.R5VK.Interfaces.Application
  SOURCES
  ../xrRender/ApplicationRender.h
  ../xrRender/dxApplicationRender.cpp
  ../xrRender/dxApplicationRender.h
)

add_module(XRay.Render.R5VK.Interfaces.Console
  SOURCES
  ../xrRender/ConsoleRender.h
  ../xrRender/dxConsoleRender.cpp
  ../xrRender/dxConsoleRender.h
)

add_module(XRay.Render.R5VK.Interfaces.Debug
  SOURCES
  ../xrRender/DebugShader.h
)

add_module(XRay.Render.R5VK.Interfaces.Debug.Render
  SOURCES
  ../xrRender/DebugRender.h
  ../xrRender/dxDebugRender.cpp
  ../xrRender/dxDebugRender.h
)

add_module(XRay.Render.R5VK.Interfaces.Environment
  SOURCES
  ../xrRender/EnvironmentRender.h
  ../xrRender/dxEnvironmentRender.cpp
  ../xrRender/dxEnvironmentRender.h
)

add_module(XRay.Render.R5VK.Interfaces.FactoryPtr
  SOURCES
  ../xrRender/FactoryPtr.h
)

add_module(XRay.Render.R5VK.Interfaces.Font
  SOURCES
  ../xrRender/FontRender.h
  ../xrRender/dxFontRender.cpp
  ../xrRender/dxFontRender.h
)

add_module(XRay.Render.R5VK.Interfaces.ImGui
  SOURCES
  ../xrRender/ImGuiRender.h
  ../xrRender/dxImGuiRender.cpp
  ../xrRender/dxImGuiRender.h
)

add_module(XRay.Render.R5VK.Interfaces.LensFlare
  SOURCES
  ../xrRender/LensFlareRender.h
  ../xrRender/dxLensFlareRender.cpp
  ../xrRender/dxLensFlareRender.h
)

add_module(XRay.Render.R5VK.Interfaces.MSAA
  SOURCES
  ../xrRenderDX10/MSAA/dx10MSAABlender.cpp
  ../xrRenderDX10/MSAA/dx10MSAABlender.h
)

add_module(XRay.Render.R5VK.Interfaces.ObjectSpace
  SOURCES
  ../xrRender/ObjectSpaceRender.h
  ../xrRender/dxObjectSpaceRender.cpp
  ../xrRender/dxObjectSpaceRender.h
)

add_module(XRay.Render.R5VK.Interfaces.Rain
  SOURCES
  ../xrRender/RainRender.h
  ../xrRender/dxRainRender.cpp
  ../xrRender/dxRainRender.h
)

add_module(XRay.Render.R5VK.Interfaces.RenderDevice
  SOURCES
  ../xrRender/RenderDeviceRender.h
  ../xrRender/dxRenderDeviceRender.cpp
  ../xrRender/dxRenderDeviceRender.h
)

add_module(XRay.Render.R5VK.Interfaces.RenderFactory
  SOURCES
  ../xrRender/RenderFactory.h
  ../xrRender/dxRenderFactory.cpp
  ../xrRender/dxRenderFactory.h
)

add_module(XRay.Render.R5VK.Interfaces.StatGraph
  SOURCES
  ../xrRender/StatGraphRender.h
  ../xrRender/dxStatGraphRender.cpp
  ../xrRender/dxStatGraphRender.h
)

add_module(XRay.Render.R5VK.Interfaces.Stats
  SOURCES
  ../xrRender/StatsRender.h
  ../xrRender/dxStatsRender.cpp
  ../xrRender/dxStatsRender.h
)

add_module(XRay.Render.R5VK.Interfaces.ThunderboltDesc
  SOURCES
  ../xrRender/ThunderboltDescRender.h
  ../xrRender/dxThunderboltDescRender.cpp
  ../xrRender/dxThunderboltDescRender.h
)

add_module(XRay.Render.R5VK.Interfaces.Thunderbolt
  SOURCES
  ../xrRender/ThunderboltRender.h
  ../xrRender/dxThunderboltRender.cpp
  ../xrRender/dxThunderboltRender.h
)

add_module(XRay.Render.R5VK.Interfaces.UI.Render
  SOURCES
  ../xrRender/UIRender.h
  ../xrRender/dxUIRender.cpp
  ../xrRender/dxUIRender.h
)

add_module(XRay.Render.R5VK.Interfaces.UI.SequenceVideoItem
  SOURCES
  ../xrRender/UISequenceVideoItem.h
  ../xrRender/dxUISequenceVideoItem.cpp
  ../xrRender/dxUISequenceVideoItem.h
)

add_module(XRay.Render.R5VK.Interfaces.UI.Shader
  SOURCES
  ../xrRender/UIShader.h
  ../xrRender/dxUIShader.cpp
  ../xrRender/dxUIShader.h
)

add_module(XRay.Render.R5VK.Interfaces.WallMarkArray
  SOURCES
  ../xrRender/WallMarkArray.h
  ../xrRender/dxWallMarkArray.cpp
  ../xrRender/dxWallMarkArray.h
)

add_module(XRay.Render.R5VK.Lights
  SOURCES
  ../xrRender/light.cpp
  ../xrRender/light.h

  ../xrRender/Light_DB.cpp
  ../xrRender/Light_DB.h

  ../xrRenderPC_R4/light_GI.cpp
  ../xrRenderPC_R4/light_gi.h

  ../xrRender/Light_Package.cpp
  ../xrRender/Light_Package.h

  ../xrRenderPC_R4/Light_Render_Direct_ComputeXFS.cpp
  ../xrRenderPC_R4/Light_Render_Direct.cpp
  ../xrRenderPC_R4/Light_Render_Direct.h

  ../xrRenderPC_R4/light_smapvis.cpp
  ../xrRenderPC_R4/light_smapvis.h

  ../xrRenderPC_R4/light_vis.cpp

  ../xrRender/LightTrack.cpp
  ../xrRender/LightTrack.h

  ../xrRenderPC_R4/SMAP_Allocator.h
)

add_module(XRay.Render.R5VK.Models
  SOURCES
  ../xrRender/ModelPool.cpp
  ../xrRender/ModelPool.h
)

add_module(XRay.Render.R5VK.Models.Visuals
  SOURCES
  ../xrRenderDX10/3DFluid/dx103DFluidVolume.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidVolume.h

  ../xrRender/FLOD.cpp
  ../xrRender/FLOD.h

  ../xrRender/FProgressive.cpp
  ../xrRender/FProgressive.h

  ../xrRender/FSkinned.cpp
  ../xrRender/FSkinned.h

  ../xrRender/FTreeVisual.cpp
  ../xrRender/FTreeVisual.h

  ../xrRender/FVisual.cpp
  ../xrRender/FVisual.h

  ../xrRender/ParticleEffect.cpp
  ../xrRender/ParticleEffect.h

  ../xrRender/ParticleEffectActions.cpp
  ../xrRender/ParticleEffectActions.h

  ../xrRender/ParticleEffectDef.cpp
  ../xrRender/ParticleEffectDef.h

  ../xrRender/ParticleGroup.cpp
  ../xrRender/ParticleGroup.h
)

add_module(XRay.Render.R5VK.Refactored.Backend
  SOURCES
  ../xrRenderDX10/dx10R_Backend_Runtime.h

  ../xrRender/FVF.h

  ../xrRender/R_Backend.cpp
  ../xrRender/R_Backend.h

  ../xrRender/R_Backend_DBG.cpp

  ../xrRender/R_Backend_hemi.cpp
  ../xrRender/R_Backend_hemi.h

  ../xrRender/R_Backend_Runtime.cpp
  ../xrRender/R_Backend_Runtime.h

  ../xrRender/R_Backend_tree.cpp
  ../xrRender/R_Backend_tree.h

  ../xrRender/R_Backend_xform.cpp
  ../xrRender/R_Backend_xform.h

  ../xrRender/R_DStreams.cpp
  ../xrRender/R_DStreams.h

  ../xrRenderPC_R4/R_Backend_LOD.cpp
  ../xrRenderPC_R4/R_Backend_LOD.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.DebugDraw
  SOURCES
  ../xrRender/D3DUtils.cpp
  ../xrRender/D3DUtils.h

  ../xrRender/DrawUtils.h

  ../xrRender/du_box.cpp
  ../xrRender/du_box.h

  ../xrRender/du_cone.cpp
  ../xrRender/du_cone.h

  ../xrRender/du_cylinder.cpp
  ../xrRender/du_cylinder.h

  ../xrRender/du_sphere.cpp
  ../xrRender/du_sphere.h

  ../xrRender/du_sphere_part.cpp
  ../xrRender/du_sphere_part.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Gamma
  SOURCES
  ../xrRender/xr_effgamma.cpp
  ../xrRender/xr_effgamma.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Shaders.Blender
  SOURCES
  ../xrRender/blenders/Blender.cpp
  ../xrRender/blenders/Blender.h

  ../xrRender/blenders/Blender_CLSID.h

  ../xrRender/blenders/Blender_Palette.cpp

  ../xrRender/blenders/Blender_Recorder.cpp
  ../xrRender/blenders/Blender_Recorder.h

  ../xrRender/Blender_Recorder_R2.cpp
  ../xrRenderDX10/Blender_Recorder_R3.cpp
  ../xrRender/Blender_Recorder_StandartBinding.cpp

  ../xrRender/tss_def.cpp
  ../xrRender/tss_def.h

  ../xrRender/tss.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Shaders.Resources
  SOURCES
  ../xrRenderPC_R4/ComputeShader.cpp
  ../xrRenderPC_R4/ComputeShader.h

  ../xrRenderPC_R4/CSCompiler.cpp
  ../xrRenderPC_R4/CSCompiler.h

  ../xrRenderDX10/dx10ConstantBuffer.cpp
  ../xrRenderDX10/dx10ConstantBuffer.h
  ../xrRenderDX10/dx10ConstantBuffer_impl.h
  ../xrRenderDX10/dx10r_constants.cpp
  ../xrRenderDX10/dx10SH_RT.cpp
  ../xrRenderDX10/dx10SH_Texture.cpp
  ../xrRender/r_constants.cpp
  ../xrRender/r_constants.h
  ../xrRender/SH_Atomic.cpp
  ../xrRender/SH_Constant.cpp
  ../xrRender/SH_Matrix.cpp
  ../xrRender/SH_Atomic.h
  ../xrRender/SH_Constant.h
  ../xrRender/SH_Matrix.h
  ../xrRender/SH_RT.h
  ../xrRender/SH_Texture.h

  ../xrRender/Shader.cpp
  ../xrRender/Shader.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Shaders.Resources.DX10RShader
  SOURCES
  ../xrRenderDX10/dx10r_constants_cache.cpp
  ../xrRenderDX10/dx10r_constants_cache.h
  ../xrRender/r_constants_cache.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Shaders.Manager
  SOURCES
  ../../xrEngine/ai_script_lua_debug.cpp
  ../../xrEngine/ai_script_lua_extension.cpp
  ../xrRenderDX10/dx10ResourceManager_Resources.cpp
  ../xrRenderDX10/dx10ResourceManager_Scripting.cpp
  ../xrRender/ETextureParams.cpp
  ../xrRender/ETextureParams.h
  ../xrRender/ResourceManager.cpp
  ../xrRender/ResourceManager.h
  ../xrRender/ResourceManager_Loader.cpp
  ../xrRender/ResourceManager_Reset.cpp
  ../xrRender/ShaderResourceTraits.h
  ../xrRender/TextureDescrManager.cpp
  ../xrRender/TextureDescrManager.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Visuals
  SOURCES
  ../xrRender/dxParticleCustom.cpp
  ../xrRender/dxParticleCustom.h

  ../xrRender/FBasicVisual.cpp
  ../xrRender/FBasicVisual.h

  ../xrRender/FHierrarhyVisual.cpp
  ../xrRender/FHierrarhyVisual.h

  ../xrRender/ParticleCustom.h
  ../xrRender/RenderVisual.h
)

add_module(XRay.Render.R5VK.Refactored.Execution3D.Skeleton
  SOURCES
  ../xrRender/Animation.cpp
  ../xrRender/Animation.h
  ../xrRender/Kinematics.h
  ../xrRender/KinematicsAnimated.h
  ../xrRender/SkeletonAnimated.cpp
  ../xrRender/SkeletonAnimated.h
  ../xrRender/SkeletonCustom.cpp
  ../xrRender/SkeletonCustom.h
  ../xrRender/SkeletonRigid.cpp
  ../xrRender/SkeletonX.cpp
  ../xrRender/SkeletonX.h
)

add_module(XRay.Render.R5VK.Refactored.HW
  SOURCES
  ../xrRenderVK/vkAllocator.cpp
  ../xrRenderVK/vkAllocator.h
  ../xrRenderVK/vkFrame.cpp
  ../xrRenderVK/vkFrame.h
  ../xrRenderVK/vkSwapchain.cpp
  ../xrRenderVK/vkSwapchain.h
  ../xrRenderVK/vkHW.cpp
  ../xrRender/HW.h
  ../xrRender/HWCaps.cpp
  ../xrRender/HWCaps.h
)

add_module(XRay.Render.R5VK.Refactored.Interfaces
  SOURCES
  ../xrRender/IRenderDetailModel.h
  ../xrRender/RenderDetailModel.h
)

add_module(XRay.Render.R5VK.Refactored.StatsManager
  SOURCES
  ../xrRender/stats_manager.cpp
  ../xrRender/stats_manager.h
)

add_module(XRay.Render.R5VK.ShadingTemplates
  SOURCES
  ../xrRenderPC_R4/blender_bloom_build.cpp
  ../xrRenderPC_R4/blender_blur.cpp
  ../xrRenderPC_R4/blender_combine.cpp
  ../xrRenderPC_R4/blender_deffer_aref.cpp
  ../xrRenderPC_R4/blender_deffer_flat.cpp
  ../xrRenderPC_R4/blender_deffer_model.cpp
  ../xrRenderPC_R4/blender_dof.cpp
  ../xrRenderPC_R4/blender_hdr10_bloom.cpp
  ../xrRenderPC_R4/blender_hdr10_lens_flare.cpp
  ../xrRenderPC_R4/blender_lut.cpp
  ../xrRenderPC_R4/blender_nightvision.cpp
  ../xrRenderPC_R4/blender_gasmask_drops.cpp
  ../xrRenderPC_R4/blender_gasmask_dudv.cpp
  ../xrRenderPC_R4/blender_light_direct.cpp
  ../xrRenderPC_R4/blender_light_mask.cpp
  ../xrRenderPC_R4/blender_light_occq.cpp
  ../xrRenderPC_R4/blender_light_point.cpp
  ../xrRenderPC_R4/blender_light_reflected.cpp
  ../xrRenderPC_R4/blender_light_spot.cpp
  ../xrRenderPC_R4/blender_luminance.cpp
  ../xrRenderPC_R4/blender_pp_bloom.cpp
  ../xrRenderPC_R4/blender_smaa.cpp
  ../xrRenderPC_R4/blender_ssao.cpp
  ../xrRenderPC_R4/blender_ss_sunshafts.cpp
  ../xrRenderPC_R4/blender_bloom_build.h
  ../xrRenderPC_R4/blender_blur.h
  ../xrRenderPC_R4/blender_combine.h
  ../xrRenderPC_R4/blender_deffer_aref.h
  ../xrRenderPC_R4/blender_deffer_flat.h
  ../xrRenderPC_R4/blender_deffer_model.h
  ../xrRenderPC_R4/blender_dof.h
  ../xrRenderPC_R4/blender_hdr10_bloom.h
  ../xrRenderPC_R4/blender_hdr10_lens_flare.h
  ../xrRenderPC_R4/blender_lut.h
  ../xrRenderPC_R4/blender_nightvision.h
  ../xrRenderPC_R4/blender_gasmask_drops.h
  ../xrRenderPC_R4/blender_gasmask_dudv.h
  ../xrRenderPC_R4/blender_light_direct.h
  ../xrRenderPC_R4/blender_light_mask.h
  ../xrRenderPC_R4/blender_light_occq.h
  ../xrRenderPC_R4/blender_light_point.h
  ../xrRenderPC_R4/blender_light_reflected.h
  ../xrRenderPC_R4/blender_light_spot.h
  ../xrRenderPC_R4/blender_luminance.h
  ../xrRenderPC_R4/blender_pp_bloom.h
  ../xrRenderPC_R4/blender_smaa.h
  ../xrRenderPC_R4/blender_ssao.h
  ../xrRenderPC_R4/blender_ss_sunshafts.h

  ../xrRenderPC_R4/dx11HDAOCSBlender.cpp
  ../xrRenderPC_R4/dx11HDAOCSBlender.h

  ../xrRenderPC_R4/dx11MinMaxSMBlender.cpp
  ../xrRenderPC_R4/dx11MinMaxSMBlender.h

  ../xrRender/uber_deffer.cpp
  ../xrRender/uber_deffer.h
)

add_module(XRay.Render.R5VK.ShadingTemplates.3DFluid
  SOURCES
  ../xrRenderDX10/3DFluid/dx103DFluidBlenders.cpp
  ../xrRenderDX10/3DFluid/dx103DFluidBlenders.h
)

add_module(XRay.Render.R5VK.ShadingTemplates.DX10Rain
  SOURCES
  "../xrRenderDX10/DX10 Rain/dx10RainBlender.cpp"
  "../xrRenderDX10/DX10 Rain/dx10RainBlender.h"
)

add_module(XRay.Render.R5VK.ShadingTemplates.R1
  SOURCES
  ../xrRender/Blender_detail_still.cpp
  ../xrRender/Blender_detail_still.h

  "../xrRender/Blender_Lm(EbB).cpp"
  "../xrRender/Blender_Lm(EbB).h"

  ../xrRender/Blender_Model_EbB.cpp
  ../xrRender/Blender_Model_EbB.h

  ../xrRender/Blender_Screen_SET.cpp
  ../xrRender/Blender_Screen_SET.h

  ../xrRender/Blender_tree.cpp
  ../xrRender/Blender_tree.h

  ../xrRender/Blender_BmmD.cpp
  ../xrRender/Blender_BmmD.h

  ../xrRender/Blender_Editor_Selection.cpp
  ../xrRender/Blender_Editor_Selection.h

  ../xrRender/Blender_Editor_Wire.cpp
  ../xrRender/Blender_Editor_Wire.h

  ../xrRender/Blender_Particle.cpp
  ../xrRender/Blender_Particle.h
)

add_module(XRay.Render.R5VK.StateManager
  SOURCES
  ../xrRenderDX10/StateManager/dx10SamplerStateCache.cpp
  ../xrRenderDX10/StateManager/dx10ShaderResourceStateCache.cpp
  ../xrRenderDX10/StateManager/dx10State.cpp
  ../xrRenderDX10/StateManager/dx10StateCache.cpp
  ../xrRenderDX10/StateManager/dx10StateManager.cpp
  ../xrRenderDX10/StateManager/dx10SamplerStateCache.h
  ../xrRenderDX10/StateManager/dx10ShaderResourceStateCache.h
  ../xrRenderDX10/StateManager/dx10State.h
  ../xrRenderDX10/StateManager/dx10StateCache.h
  ../xrRenderDX10/StateManager/dx10StateCacheImpl.h
  ../xrRenderDX10/StateManager/dx10StateManager.h
)

add_module(XRay.Render.R5VK.Stripifier
  SOURCES
  ../xrRender/NvTriStrip.cpp
  ../xrRender/NvTriStripObjects.cpp
  ../xrRender/NvTriStrip.h
  ../xrRender/NvTriStripObjects.h
  ../xrRender/VertexCache.cpp
  ../xrRender/VertexCache.h
  ../xrRender/xrStripify.cpp
  ../xrRender/xrStripify.h
)

add_module(XRay.Render.R5VK.Utils
  SOURCES
  ../xrRender/Utils/dxHashHelper.cpp
  ../xrRender/Utils/dxHashHelper.h
)

add_module(XRay.Render.R5VK.Visibility.HOM
  SOURCES
  ../xrRender/HOM.cpp
  ../xrRender/HOM.h

  ../xrRender/occRasterizer.cpp
  ../xrRender/occRasterizer_core.cpp
  ../xrRender/occRasterizer.h
)

add_module(XRay.Render.R5VK.Visibility.Sector
  SOURCES
  ../xrRender/r__sector.cpp
  ../xrRender/r__sector_traversal.cpp
  ../xrRender/r__sector.h
)

add_module(XRay.Render.R5VK.Wallmarks
  SOURCES
  ../xrRender/WallmarksEngine.cpp
  ../xrRender/WallmarksEngine.h
)

target_compile_options(XRay.Render.R5VK
  PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/Zm113>
)

set_source_files_properties(
  ../xrRender/FLOD.cpp
  ../xrRender/occRasterizer_core.cpp
  ../xrRender/r_constants.cpp
  ../xrRender/ParticleEffectActions.cpp
  ../xrRender/ParticleEffectDef.cpp
  ../xrRender/PSLibrary.cpp
  PROPERTIES
  SKIP_UNITY_BUILD_INCLUSION true
)

# vulkan-1.dll must be DELAY-loaded (propagated to the Anomaly.Vulkan exe via INTERFACE).
# As a normal startup import, vulkan-1.dll and the Vulkan ICD it pulls in reserve the low
# address space that LuaJIT's x64 allocator (lj_alloc_create) requires, crashing the Vulkan
# build on boot before any logging. Delay-loading defers the DLL until the first Vulkan call
# during render-device init — after LuaJIT's heap is established. delayimp provides the thunk.
if(MSVC)
  target_link_options(XRay.Render.R5VK INTERFACE "/DELAYLOAD:vulkan-1.dll")
  target_link_libraries(XRay.Render.R5VK INTERFACE delayimp.lib)
endif()
