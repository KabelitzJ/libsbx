using Sbx.Managed.Interop;
using Sbx.Core.Math;

namespace Sbx.Core
{

  internal static unsafe class InternalCalls
  {
    internal static delegate* unmanaged<Logger.Level, NativeString, void> Log_LogMessage;

    internal static delegate* unmanaged<ulong, NativeString, void> Scripting_AttachScript;
    internal static delegate* unmanaged<ulong, NativeString, NativeInstance<object>> Scripting_GetInstance;

    internal static delegate* unmanaged<ulong, ReflectionType, void> Behavior_AddComponent;
		internal static delegate* unmanaged<ulong, ReflectionType, bool> Behavior_HasComponent;
		internal static delegate* unmanaged<ulong, ReflectionType, bool> Behavior_RemoveComponent;

    internal static delegate* unmanaged<ulong, NativeString> Tag_GetTag;
    internal static delegate* unmanaged<ulong, NativeString, void> Tag_SetTag;
    internal static delegate* unmanaged<ulong, byte> Node_GetLayer;
    internal static delegate* unmanaged<ulong, byte, void> Node_SetLayer;

    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_GetPosition;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_SetPosition;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_GetWorldPosition;
    internal static delegate* unmanaged<ulong, Quaternion*, void> Transform_GetRotation;
    internal static delegate* unmanaged<ulong, Quaternion*, void> Transform_SetRotation;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_GetRight;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_GetForward;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_GetUp;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_GetScale;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_SetScale;
    internal static delegate* unmanaged<ulong, Vector3*, void> Transform_LookAt;

    internal static delegate* unmanaged<ulong, bool> Animator_GetPlaying;
    internal static delegate* unmanaged<ulong, bool, void> Animator_SetPlaying;
    internal static delegate* unmanaged<ulong, NativeString> Animator_GetCurrentStateName;
    internal static delegate* unmanaged<ulong, NativeString, float, void> Animator_SetFloat;
    internal static delegate* unmanaged<ulong, NativeString, bool, void> Animator_SetBool;
    internal static delegate* unmanaged<ulong, NativeString, int, void> Animator_SetInt;
    internal static delegate* unmanaged<ulong, NativeString, void> Animator_SetTrigger;
    internal static delegate* unmanaged<ulong, NativeString, float> Animator_GetFloat;
    internal static delegate* unmanaged<ulong, NativeString, bool> Animator_GetBool;
    internal static delegate* unmanaged<ulong, NativeString, int> Animator_GetInt;

    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_GetLinearVelocity;
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_SetLinearVelocity;
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_GetAngularVelocity;
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_SetAngularVelocity;
    internal static delegate* unmanaged<ulong, float*, void> Rigidbody_GetMass;
    internal static delegate* unmanaged<ulong, float, void> Rigidbody_SetMass;
    internal static delegate* unmanaged<ulong, float*, void> Rigidbody_GetGravityScale;
    internal static delegate* unmanaged<ulong, float, void> Rigidbody_SetGravityScale;
    internal static delegate* unmanaged<ulong, uint> Rigidbody_GetCollisionDetection;
    internal static delegate* unmanaged<ulong, uint, void> Rigidbody_SetCollisionDetection;
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_AddForce;
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_AddTorque;

    internal static delegate* unmanaged<NativeString, ulong> Node_FindByName;
    internal static delegate* unmanaged<NativeString, ulong> Node_Create;
    internal static delegate* unmanaged<NativeString, ulong, ulong> Node_InstantiatePrefab;
    internal static delegate* unmanaged<ulong, void> Node_Destroy;
    internal static delegate* unmanaged<ulong, ulong, void> Node_SetParent;
    internal static delegate* unmanaged<ulong, bool, void> Node_SetActive;
    internal static delegate* unmanaged<ulong, bool> Node_GetIsActive;

    internal static delegate* unmanaged<NativeString, void> Scene_Save;
    internal static delegate* unmanaged<void> Scene_New;
    internal static delegate* unmanaged<NativeString, uint, uint*, uint> Scene_LoadByName;
    internal static delegate* unmanaged<int, uint, uint*, uint> Scene_LoadByIndex;
    internal static delegate* unmanaged<int> Scene_CountInBuildSettings;
    internal static delegate* unmanaged<NativeString, uint, uint*, uint> Scene_LoadAsyncByName;
    internal static delegate* unmanaged<int, uint, uint*, uint> Scene_LoadAsyncByIndex;
    internal static delegate* unmanaged<uint, float> Scene_GetOperationProgress;
    internal static delegate* unmanaged<uint, bool> Scene_GetAllowActivation;
    internal static delegate* unmanaged<uint, uint, void> Scene_SetAllowActivation;
    internal static delegate* unmanaged<uint, uint> Scene_Unload;
    internal static delegate* unmanaged<uint, bool> Scene_IsOperationDone;
    internal static delegate* unmanaged<int> Scene_GetCount;
    internal static delegate* unmanaged<int, uint> Scene_GetAt;
    internal static delegate* unmanaged<uint> Scene_GetActive;
    internal static delegate* unmanaged<uint, bool> Scene_SetActive;
    internal static delegate* unmanaged<NativeString, uint> Scene_FindByName;
    internal static delegate* unmanaged<NativeString, uint> Scene_FindByPath;
    internal static delegate* unmanaged<int, uint> Scene_FindByBuildIndex;
    internal static delegate* unmanaged<uint, NativeString> Scene_GetName;
    internal static delegate* unmanaged<uint, NativeString> Scene_GetPath;
    internal static delegate* unmanaged<uint, int> Scene_GetBuildIndex;
    internal static delegate* unmanaged<uint, bool> Scene_IsLoaded;
    internal static delegate* unmanaged<uint, ulong*, uint, uint> Scene_GetRootNodes;
    internal static delegate* unmanaged<ulong, uint, bool> Scene_MoveNode;
    internal static delegate* unmanaged<ulong, void> Node_DontDestroyOnLoad;

    internal static delegate* unmanaged<ulong, NativeString, void> ParticleEffect_Load;
    internal static delegate* unmanaged<ulong, void> ParticleEffect_Play;
    internal static delegate* unmanaged<ulong, void> ParticleEffect_Pause;
    internal static delegate* unmanaged<ulong, void> ParticleEffect_Stop;
    internal static delegate* unmanaged<ulong, bool> ParticleEffect_GetLoop;
    internal static delegate* unmanaged<ulong, bool, void> ParticleEffect_SetLoop;
    internal static delegate* unmanaged<ulong, bool> ParticleEffect_GetIsPlaying;

    internal static delegate* unmanaged<NativeString, ulong> AudioClip_Load;
    internal static delegate* unmanaged<ulong, bool> AudioClip_IsLoaded;
    internal static delegate* unmanaged<ulong, float> AudioClip_GetLength;
    internal static delegate* unmanaged<ulong, uint> AudioClip_GetChannels;
    internal static delegate* unmanaged<ulong, uint> AudioClip_GetFrequency;

    internal static delegate* unmanaged<ulong, Components.AudioSourceSettings*, void> AudioSource_GetSettings;
    internal static delegate* unmanaged<ulong, Components.AudioSourceSettings*, void> AudioSource_SetSettings;
    internal static delegate* unmanaged<ulong, ulong> AudioSource_GetClip;
    internal static delegate* unmanaged<ulong, ulong, void> AudioSource_SetClip;
    internal static delegate* unmanaged<ulong, NativeString> AudioSource_GetOutputGroup;
    internal static delegate* unmanaged<ulong, NativeString, bool> AudioSource_SetOutputGroup;
    internal static delegate* unmanaged<ulong, void> AudioSource_Play;
    internal static delegate* unmanaged<ulong, void> AudioSource_Pause;
    internal static delegate* unmanaged<ulong, void> AudioSource_UnPause;
    internal static delegate* unmanaged<ulong, void> AudioSource_Stop;
    internal static delegate* unmanaged<ulong, bool> AudioSource_GetIsPlaying;
    internal static delegate* unmanaged<ulong, float> AudioSource_GetTime;
    internal static delegate* unmanaged<ulong, float, void> AudioSource_SetTime;
    internal static delegate* unmanaged<ulong, ulong, float, void> AudioSource_PlayOneShot;

    internal static delegate* unmanaged<ulong, float> AudioListener_GetVolume;
    internal static delegate* unmanaged<ulong, float, void> AudioListener_SetVolume;

    internal static delegate* unmanaged<ulong, Vector3*, float, void> Audio_PlayClipAtPoint;
    internal static delegate* unmanaged<NativeString, float, bool> Audio_SetMixerGroupVolume;
    internal static delegate* unmanaged<NativeString, float> Audio_GetMixerGroupVolume;
    internal static delegate* unmanaged<NativeString, void> Audio_ResetMixerGroupVolume;

    internal static delegate* unmanaged<ulong, Physics.CharacterControllerSettings*, void> CharacterController_GetSettings;
    internal static delegate* unmanaged<ulong, Physics.CharacterControllerSettings*, void> CharacterController_SetSettings;
    internal static delegate* unmanaged<ulong, bool> CharacterController_GetIsGrounded;
    internal static delegate* unmanaged<ulong, uint> CharacterController_GetFlags;
    internal static delegate* unmanaged<ulong, Vector3*, void> CharacterController_GetVelocity;
    internal static delegate* unmanaged<ulong, Vector3*, uint> CharacterController_Move;
    internal static delegate* unmanaged<ulong, Vector3*, bool> CharacterController_SimpleMove;

    internal static delegate* unmanaged<KeyCode, Bool32> Input_IsKeyPressed;
		internal static delegate* unmanaged<KeyCode, Bool32> Input_IsKeyDown;
    internal static delegate* unmanaged<KeyCode, Bool32> Input_IsKeyReleased;
		internal static delegate* unmanaged<MouseButton, bool> Input_IsMouseButtonPressed;
		internal static delegate* unmanaged<MouseButton, bool> Input_IsMouseButtonDown;
    internal static delegate* unmanaged<MouseButton, bool> Input_IsMouseButtonReleased;
    internal static delegate* unmanaged<Vector2*, void> Input_MousePosition;
    internal static delegate* unmanaged<Vector2*, void> Input_ScrollDelta;

    internal static delegate* unmanaged<Ray*, Vector2*, void> Camera_ScreenPointToRay;
    internal static delegate* unmanaged<Vector3*, Vector2*, bool> Camera_WorldToScreenPoint;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetPosition;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainSetPosition;
    internal static delegate* unmanaged<Quaternion*, void> Camera_MainGetRotation;
    internal static delegate* unmanaged<Quaternion*, void> Camera_MainSetRotation;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetForward;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetRight;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetUp;

    internal static delegate* unmanaged<Vector2*, void> Camera_GetViewport;
    internal static delegate* unmanaged<Vector2*, void> Camera_GetViewportOffset;
    internal static delegate* unmanaged<bool, void> RenderSettings_SetWireframeEnabled;
    internal static delegate* unmanaged<bool> RenderSettings_GetWireframeEnabled;
    internal static delegate* unmanaged<uint> RenderSettings_GetMsaaSamples;
    internal static delegate* unmanaged<uint, void> RenderSettings_SetMsaaSamples;

    internal static delegate* unmanaged<ulong, float*, void> Camera_GetFovDegrees;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetFovDegrees;
    internal static delegate* unmanaged<ulong, float*, void> Camera_GetNearPlane;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetNearPlane;
    internal static delegate* unmanaged<ulong, float*, void> Camera_GetFarPlane;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetFarPlane;
    internal static delegate* unmanaged<ulong, float*, void> Camera_GetExposure;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetExposure;
    internal static delegate* unmanaged<ulong, PostProcessSettings*, void> Camera_GetPostProcess;
    internal static delegate* unmanaged<ulong, PostProcessSettings*, void> Camera_SetPostProcess;

    internal static delegate* unmanaged<float*, void> Time_DeltaTime;
    internal static delegate* unmanaged<float> Time_FixedDeltaTime;

    internal static delegate* unmanaged<Ray*, float, uint, ulong*, Vector3*, Vector3*, float*, bool> Physics_Raycast;
    internal static delegate* unmanaged<Vector3*, float, uint, ulong*, uint, uint> Physics_OverlapSphere;
    internal static delegate* unmanaged<Physics.PhysicsShape*, Vector3*, float, uint, ulong*, Vector3*, Vector3*, float*, bool> Physics_ShapeCast;
    internal static delegate* unmanaged<Physics.PhysicsShape*, uint, ulong*, uint, uint> Physics_OverlapShape;

    internal static delegate* unmanaged<Vector3*, Vector3*, Color*, void> DebugDraw_Line;
    internal static delegate* unmanaged<Vector3*, float, Color*, void> DebugDraw_WireSphere;
    internal static delegate* unmanaged<Vector3*, Quaternion*, Vector3*, Color*, void> DebugDraw_WireBox;
    internal static delegate* unmanaged<Vector3*, Vector3*, float, Color*, void> DebugDraw_WireCircle;

    internal static delegate* unmanaged<float, float, float, float, float, float, float, float, float, float, int, bool> Nav_Bake;
    internal static delegate* unmanaged<bool> Nav_HasNavMesh;
    internal static delegate* unmanaged<Vector3*, Vector3*, bool> Nav_SamplePosition;

    internal static delegate* unmanaged<ulong, Vector3*, bool> NavAgent_SetDestination;
    internal static delegate* unmanaged<ulong, byte> NavAgent_GetState;
    internal static delegate* unmanaged<ulong, Vector3*, void> NavAgent_GetVelocity;
    internal static delegate* unmanaged<ulong, float*, void> NavAgent_GetRemainingDistance;
    internal static delegate* unmanaged<ulong, float*, void> NavAgent_GetRadius;
    internal static delegate* unmanaged<ulong, float, void> NavAgent_SetRadius;

    internal static delegate* unmanaged<ulong, float*, void> NavAgent_GetHeight;
    internal static delegate* unmanaged<ulong, float, void> NavAgent_SetHeight;

    internal static delegate* unmanaged<ulong, float*, void> NavAgent_GetBaseOffset;
    internal static delegate* unmanaged<ulong, float, void> NavAgent_SetBaseOffset;
    internal static delegate* unmanaged<ulong, float*, void> NavAgent_GetSpeed;
    internal static delegate* unmanaged<ulong, float, void> NavAgent_SetSpeed;
    internal static delegate* unmanaged<ulong, float*, void> NavAgent_GetAcceleration;
    internal static delegate* unmanaged<ulong, float, void> NavAgent_SetAcceleration;

    internal static delegate* unmanaged<uint, uint, float, float, float, uint, void> Terrain_Generate;
    internal static delegate* unmanaged<NativeString, float, float, bool> Terrain_Load;
    internal static delegate* unmanaged<Vector2*, float*, void> Terrain_SampleHeight;
    internal static delegate* unmanaged<Vector2*, Vector3*, void> Terrain_SampleNormal;
    internal static delegate* unmanaged<float, float, float, float> Math_NoiseSimplex;
    internal static delegate* unmanaged<float, float, float, uint, float, float, float> Math_NoiseFractal;

    internal static delegate* unmanaged<ulong, Vector3*, Vector3*, Vector2*, Color*, uint, uint*, uint, Color*, void> MeshRenderer_SetGeometry;
    internal static delegate* unmanaged<ulong, uint, ulong, void> MeshRenderer_SetMaterial;
    internal static delegate* unmanaged<ulong, Vector3*, Vector3*, Vector2*, Color*, uint, uint*, uint, void> InstancedMeshRenderer_SetGeometry;
    internal static delegate* unmanaged<ulong, ulong, void> InstancedMeshRenderer_SetMaterial;
    internal static delegate* unmanaged<ulong, InstanceData*, uint, void> InstancedMeshRenderer_SetInstances;
    internal static delegate* unmanaged<NativeString, ulong> Material_Load;
    internal static delegate* unmanaged<ulong, ulong> Material_CreateInstance;
    internal static delegate* unmanaged<ulong, uint, ulong, void> Material_SetTexture;
    internal static delegate* unmanaged<ulong, uint, float, float, float, float, void> Material_SetGenericParam;
    internal static delegate* unmanaged<ulong, uint, ulong, void> Material_SetGenericTexture;
    internal static delegate* unmanaged<ulong, void> Material_Release;
    internal static delegate* unmanaged<ulong, bool> Material_IsLoaded;
    internal static delegate* unmanaged<NativeString> Project_GetAssetsDirectory;
    internal static delegate* unmanaged<NativeString, uint, ulong> Texture_Load;
    internal static delegate* unmanaged<uint, uint, uint, ulong> Texture_CreateStorageImage;
    internal static delegate* unmanaged<ulong, uint, uint, uint, Color*, void> Texture_ReadPixels;
    internal static delegate* unmanaged<ulong, void> Texture_Release;
    internal static delegate* unmanaged<NativeString, uint, uint, byte*, bool> Debug_WritePng;
    internal static delegate* unmanaged<ulong, bool> Texture_IsResident;
    internal static delegate* unmanaged<ulong*, uint, uint, uint, ulong> Texture2DArray_Create;
    internal static delegate* unmanaged<ulong, bool> Texture2DArray_IsResident;
    internal static delegate* unmanaged<ulong, void> Texture2DArray_Release;
    internal static delegate* unmanaged<ulong, uint, ulong, void> Material_SetGenericTextureArray;
    internal static delegate* unmanaged<NativeString, ulong> Font_Load;
    internal static delegate* unmanaged<ulong, bool> Font_IsResident;
    internal static delegate* unmanaged<ulong, uint, FontGlyph*, bool> Font_GetGlyph;
    internal static delegate* unmanaged<ulong, Vector3*, void> Font_GetMetrics;
    internal static delegate* unmanaged<ulong, uint, ulong, void> Material_SetGenericTextureFont;

    internal static delegate* unmanaged<int, int, uint, ulong> ComputeBuffer_Create;
    internal static delegate* unmanaged<ulong, void*, int, bool> ComputeBuffer_SetData;
    internal static delegate* unmanaged<ulong, void*, int, bool> ComputeBuffer_GetData;
    internal static delegate* unmanaged<ulong, void> ComputeBuffer_Release;

    internal static delegate* unmanaged<NativeString, ulong> ComputeShader_Load;
    internal static delegate* unmanaged<ulong, NativeString, uint, void*, bool> ComputeShader_SetValue;
    internal static delegate* unmanaged<ulong, NativeString, ulong, bool, bool> ComputeShader_SetTexture;
    internal static delegate* unmanaged<ulong, NativeString, ulong, bool> ComputeShader_SetBuffer;
    internal static delegate* unmanaged<ulong, void> ComputeShader_Release;

    internal static delegate* unmanaged<ulong> ComputeCommands_Begin;
    internal static delegate* unmanaged<ulong, ulong, uint, uint, uint, bool> ComputeCommands_Dispatch;
    internal static delegate* unmanaged<ulong, bool, bool> ComputeCommands_Submit;
    internal static delegate* unmanaged<ulong, bool> ComputeCommands_IsComplete;
    internal static delegate* unmanaged<ulong, void> ComputeCommands_Release;

    internal static delegate* unmanaged<ulong, int*, void> Canvas_GetSortOrder;
    internal static delegate* unmanaged<ulong, int, void> Canvas_SetSortOrder;
    internal static delegate* unmanaged<ulong, uint*, void> Canvas_GetMode;
    internal static delegate* unmanaged<ulong, uint, void> Canvas_SetMode;
    internal static delegate* unmanaged<ulong, bool> Canvas_GetBillboard;
    internal static delegate* unmanaged<ulong, bool, void> Canvas_SetBillboard;
    internal static delegate* unmanaged<ulong, float*, void> Canvas_GetWorldScale;
    internal static delegate* unmanaged<ulong, float, void> Canvas_SetWorldScale;
    internal static delegate* unmanaged<bool> Canvas_WantsPointerCapture;

    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_GetAnchorMin;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_SetAnchorMin;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_GetAnchorMax;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_SetAnchorMax;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_GetAnchoredPosition;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_SetAnchoredPosition;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_GetSizeDelta;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_SetSizeDelta;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_GetPivot;
    internal static delegate* unmanaged<ulong, Vector2*, void> RectTransform_SetPivot;

    internal static delegate* unmanaged<ulong, Color*, void> UIImage_GetTint;
    internal static delegate* unmanaged<ulong, Color*, void> UIImage_SetTint;
    internal static delegate* unmanaged<ulong, NativeString, void> UIImage_LoadSprite;

    internal static delegate* unmanaged<ulong, NativeString> UIText_GetText;
    internal static delegate* unmanaged<ulong, NativeString, void> UIText_SetText;
    internal static delegate* unmanaged<ulong, float*, void> UIText_GetFontSize;
    internal static delegate* unmanaged<ulong, float, void> UIText_SetFontSize;
    internal static delegate* unmanaged<ulong, Color*, void> UIText_GetColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIText_SetColor;
    internal static delegate* unmanaged<ulong, NativeString, void> UIText_LoadFont;

    internal static delegate* unmanaged<ulong, bool> UIButton_GetInteractable;
    internal static delegate* unmanaged<ulong, bool, void> UIButton_SetInteractable;
    internal static delegate* unmanaged<ulong, Color*, void> UIButton_GetNormalColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIButton_SetNormalColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIButton_GetHoveredColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIButton_SetHoveredColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIButton_GetPressedColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIButton_SetPressedColor;
    internal static delegate* unmanaged<ulong, bool> UIButton_GetIsHovered;
    internal static delegate* unmanaged<ulong, bool> UIButton_GetIsPressed;
    internal static delegate* unmanaged<ulong, bool> UIButton_GetWasClicked;

    internal static delegate* unmanaged<ulong, float*, void> CanvasGroup_GetAlpha;
    internal static delegate* unmanaged<ulong, float, void> CanvasGroup_SetAlpha;
    internal static delegate* unmanaged<ulong, bool> CanvasGroup_GetInteractable;
    internal static delegate* unmanaged<ulong, bool, void> CanvasGroup_SetInteractable;
    internal static delegate* unmanaged<ulong, bool> CanvasGroup_GetBlocksRaycasts;
    internal static delegate* unmanaged<ulong, bool, void> CanvasGroup_SetBlocksRaycasts;
    internal static delegate* unmanaged<ulong, bool> CanvasGroup_GetIgnoreParentGroups;
    internal static delegate* unmanaged<ulong, bool, void> CanvasGroup_SetIgnoreParentGroups;

    internal static delegate* unmanaged<ulong, bool> UIToggle_GetIsOn;
    internal static delegate* unmanaged<ulong, bool, void> UIToggle_SetIsOn;
    internal static delegate* unmanaged<ulong, bool> UIToggle_GetInteractable;
    internal static delegate* unmanaged<ulong, bool, void> UIToggle_SetInteractable;

    internal static delegate* unmanaged<ulong, float*, void> UISlider_GetValue;
    internal static delegate* unmanaged<ulong, float, void> UISlider_SetValue;
    internal static delegate* unmanaged<ulong, float*, void> UISlider_GetMinValue;
    internal static delegate* unmanaged<ulong, float, void> UISlider_SetMinValue;
    internal static delegate* unmanaged<ulong, float*, void> UISlider_GetMaxValue;
    internal static delegate* unmanaged<ulong, float, void> UISlider_SetMaxValue;
    internal static delegate* unmanaged<ulong, bool> UISlider_GetWholeNumbers;
    internal static delegate* unmanaged<ulong, bool, void> UISlider_SetWholeNumbers;
    internal static delegate* unmanaged<ulong, bool> UISlider_GetInteractable;
    internal static delegate* unmanaged<ulong, bool, void> UISlider_SetInteractable;

    internal static delegate* unmanaged<ulong, float*, void> UIScrollbar_GetValue;
    internal static delegate* unmanaged<ulong, float, void> UIScrollbar_SetValue;
    internal static delegate* unmanaged<ulong, float*, void> UIScrollbar_GetSize;
    internal static delegate* unmanaged<ulong, float, void> UIScrollbar_SetSize;
    internal static delegate* unmanaged<ulong, bool> UIScrollbar_GetInteractable;
    internal static delegate* unmanaged<ulong, bool, void> UIScrollbar_SetInteractable;

    internal static delegate* unmanaged<ulong, Vector2*, void> UIScrollRect_GetNormalizedPosition;
    internal static delegate* unmanaged<ulong, Vector2*, void> UIScrollRect_SetNormalizedPosition;
    internal static delegate* unmanaged<ulong, bool> UIScrollRect_GetHorizontal;
    internal static delegate* unmanaged<ulong, bool, void> UIScrollRect_SetHorizontal;
    internal static delegate* unmanaged<ulong, bool> UIScrollRect_GetVertical;
    internal static delegate* unmanaged<ulong, bool, void> UIScrollRect_SetVertical;
    internal static delegate* unmanaged<ulong, ulong, void> UIScrollRect_SetContent;

    internal static delegate* unmanaged<ulong, bool> UIMask_GetShowMaskGraphic;
    internal static delegate* unmanaged<ulong, bool, void> UIMask_SetShowMaskGraphic;

    internal static delegate* unmanaged<ulong, uint*, uint*, void> UIText_GetAlignment;
    internal static delegate* unmanaged<ulong, uint, uint, void> UIText_SetAlignment;

    internal static delegate* unmanaged<ulong, bool, UI.LayoutGroupData*, void> LayoutGroup_Get;
    internal static delegate* unmanaged<ulong, bool, UI.LayoutGroupData*, void> LayoutGroup_Set;
    internal static delegate* unmanaged<ulong, UI.LayoutElementData*, void> LayoutElement_Get;
    internal static delegate* unmanaged<ulong, UI.LayoutElementData*, void> LayoutElement_Set;
    internal static delegate* unmanaged<ulong, uint*, uint*, void> ContentSizeFitter_Get;
    internal static delegate* unmanaged<ulong, uint, uint, void> ContentSizeFitter_Set;

  } // class InternalCalls

} // namespace Sbx.Core

