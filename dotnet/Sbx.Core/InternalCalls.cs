using Sbx.Managed.Interop;
using Sbx.Math;

namespace Sbx.Core
{

  internal static unsafe class InternalCalls
  {
    internal static delegate* unmanaged<Logger.Level, NativeString, void> Log_LogMessage;

    internal static delegate* unmanaged<ulong, ReflectionType, void> Behavior_AddComponent;
		internal static delegate* unmanaged<ulong, ReflectionType, bool> Behavior_HasComponent;
		// internal static delegate* unmanaged<ulong, ReflectionType, bool> Behavior_RemoveComponent;

    internal static delegate* unmanaged<ulong, NativeString> Tag_GetTag;
    internal static delegate* unmanaged<ulong, NativeString, void> Tag_SetTag;

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
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_AddForce;
    internal static delegate* unmanaged<ulong, Vector3*, void> Rigidbody_AddTorque;

    internal static delegate* unmanaged<NativeString, ulong> Node_FindByName;
    internal static delegate* unmanaged<NativeString, ulong> Node_Create;
    internal static delegate* unmanaged<ulong, void> Node_Destroy;
    internal static delegate* unmanaged<ulong, ulong, void> Node_SetParent;

    internal static delegate* unmanaged<ulong, NativeString, void> ParticleEffect_Load;
    internal static delegate* unmanaged<ulong, void> ParticleEffect_Play;
    internal static delegate* unmanaged<ulong, void> ParticleEffect_Pause;
    internal static delegate* unmanaged<ulong, void> ParticleEffect_Stop;
    internal static delegate* unmanaged<ulong, bool> ParticleEffect_GetLoop;
    internal static delegate* unmanaged<ulong, bool, void> ParticleEffect_SetLoop;
    internal static delegate* unmanaged<ulong, bool> ParticleEffect_GetIsPlaying;

    internal static delegate* unmanaged<ulong, float*, void> CharacterController_GetHeight;
    internal static delegate* unmanaged<ulong, float*, void> CharacterController_GetRadius;
    internal static delegate* unmanaged<ulong, float*, void> CharacterController_GetSlopeLimit;
    internal static delegate* unmanaged<ulong, float*, void> CharacterController_GetStepOffset;
    internal static delegate* unmanaged<ulong, bool> CharacterController_GetIsGrounded;
    internal static delegate* unmanaged<ulong, byte*, void> CharacterController_GetFlags;
    internal static delegate* unmanaged<ulong, Vector3*, void> CharacterController_Move;

    internal static delegate* unmanaged<KeyCode, Bool32> Input_IsKeyPressed;
		internal static delegate* unmanaged<KeyCode, Bool32> Input_IsKeyDown;
    internal static delegate* unmanaged<KeyCode, Bool32> Input_IsKeyReleased;
		internal static delegate* unmanaged<MouseButton, bool> Input_IsMouseButtonPressed;
		internal static delegate* unmanaged<MouseButton, bool> Input_IsMouseButtonDown;
    internal static delegate* unmanaged<MouseButton, bool> Input_IsMouseButtonReleased;
    internal static delegate* unmanaged<Vector2*, void> Input_MousePosition;
    internal static delegate* unmanaged<Vector2*, void> Input_ScrollDelta;

    internal static delegate* unmanaged<Ray*, Vector2*, void> Camera_ScreenPointToRay;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetPosition;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainSetPosition;
    internal static delegate* unmanaged<Quaternion*, void> Camera_MainGetRotation;
    internal static delegate* unmanaged<Quaternion*, void> Camera_MainSetRotation;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetForward;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetRight;
    internal static delegate* unmanaged<Vector3*, void> Camera_MainGetUp;

    internal static delegate* unmanaged<Vector2*, void> Camera_GetViewport;

    internal static delegate* unmanaged<ulong, float*, void> Camera_GetFovDegrees;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetFovDegrees;
    internal static delegate* unmanaged<ulong, float*, void> Camera_GetNearPlane;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetNearPlane;
    internal static delegate* unmanaged<ulong, float*, void> Camera_GetFarPlane;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetFarPlane;
    internal static delegate* unmanaged<ulong, float*, void> Camera_GetExposure;
    internal static delegate* unmanaged<ulong, float, void> Camera_SetExposure;

    internal static delegate* unmanaged<float*, void> Time_DeltaTime;

    internal static delegate* unmanaged<Ray*, float, ulong*, Vector3*, Vector3*, float*, bool> Physics_Raycast;

    internal static delegate* unmanaged<uint, uint, float, float, float, uint, void> Terrain_Generate;
    internal static delegate* unmanaged<Vector2*, float*, void> Terrain_SampleHeight;
    internal static delegate* unmanaged<Vector2*, Vector3*, void> Terrain_SampleNormal;

    internal static delegate* unmanaged<ulong, Vector3*, Vector3*, Vector2*, uint, uint*, uint, Color*, void> MeshRenderer_SetGeometry;

    internal static delegate* unmanaged<ulong, int*, void> Canvas_GetSortOrder;
    internal static delegate* unmanaged<ulong, int, void> Canvas_SetSortOrder;
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

    internal static delegate* unmanaged<ulong, NativeString> UIText_GetText;
    internal static delegate* unmanaged<ulong, NativeString, void> UIText_SetText;
    internal static delegate* unmanaged<ulong, float*, void> UIText_GetFontSize;
    internal static delegate* unmanaged<ulong, float, void> UIText_SetFontSize;
    internal static delegate* unmanaged<ulong, Color*, void> UIText_GetColor;
    internal static delegate* unmanaged<ulong, Color*, void> UIText_SetColor;

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

  } // class InternalCalls

} // namespace Sbx.Core

