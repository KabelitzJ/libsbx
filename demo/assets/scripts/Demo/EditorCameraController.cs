using Sbx.Core;
using Sbx.Math;

namespace Demo {

  public class EditorCameraController : Behavior
  {
    public float panSensitivity = 0.5f;
    public float orbitSensitivity = 0.3f;
    public float flySpeed = 5.0f;
    public float flySpeedMultiplier = 3.0f;
    public float lookSensitivity = 0.2f;
    public float scrollSensitivity = 2.0f;
    public float minOrbitDistance = 1.0f;
    public float maxOrbitDistance = 500.0f;
    public float smoothTime = 12.0f;

    private float _yaw = 0.0f;
    private float _pitch = 20.0f;
    private float _orbitDistance = 10.0f;
    private Vector3 _focalPoint = Vector3.Zero;

    private Vector3 _targetPosition;
    private Quaternion _targetRotation;
    private Vector3 _lastMousePosition;
    private bool _isFlying = false;

    public override void OnCreate()
    {
      Vector3 euler = Camera.Main.Rotation.EulerAngles();
      _yaw   = euler.Y;
      _pitch = euler.X;

      _targetPosition = Camera.Main.Position;
      _targetRotation = Camera.Main.Rotation;
      _focalPoint = Camera.Main.Position + Camera.Main.Forward * _orbitDistance;
    }

    public override void OnUpdate()
    {
      HandleScroll();
      HandlePan();
      HandleOrbit();
      HandleFreeLook();
      HandleFocus();
      ApplyMovement();
    }

    void HandleScroll()
    {
      float scroll = Input.ScrollDelta().Y;

      if (scroll == 0)
      {
        return;
      }

      if (_isFlying)
      {
        flySpeed = System.Math.Clamp(flySpeed + scroll * 0.5f, 0.5f, 100.0f);
      }
      else
      {
        _orbitDistance -= scroll * scrollSensitivity * (_orbitDistance * 0.1f);
        _orbitDistance  = System.Math.Clamp(_orbitDistance, minOrbitDistance, maxOrbitDistance);
        _targetPosition = _focalPoint - Camera.Main.Forward * _orbitDistance;
      }
    }

    void HandlePan()
    {
      if (Input.IsMouseButtonPressed(MouseButton.Middle))
      {
        _lastMousePosition = new Vector3(Input.MousePosition());
      }

      if (Input.IsMouseButtonDown(MouseButton.Middle))
      {
        Vector3 current = new Vector3(Input.MousePosition());
        Vector3 delta   = _lastMousePosition - current;
        _lastMousePosition = current;

        float scale  = _orbitDistance * 0.001f * panSensitivity;
        Vector3 pan  = (Camera.Main.Right * delta.X + Camera.Main.Up * -delta.Y) * scale;

        _focalPoint     += pan;
        _targetPosition += pan;
      }
    }

    void HandleOrbit()
    {
      bool altHeld = Input.IsKeyDown(KeyCode.LeftAlt) || Input.IsKeyDown(KeyCode.RightAlt);

      if (altHeld && Input.IsMouseButtonPressed(MouseButton.Left))
      {
        _lastMousePosition = new Vector3(Input.MousePosition());
      }

      if (altHeld && Input.IsMouseButtonDown(MouseButton.Left))
      {
        Vector3 current = new Vector3(Input.MousePosition());
        Vector3 delta   = _lastMousePosition - current;
        _lastMousePosition = current;

        _yaw   += delta.X * orbitSensitivity;
        _pitch  = System.Math.Clamp(_pitch + delta.Y * orbitSensitivity, -89.0f, 89.0f);

        UpdateOrbitPosition();
      }
    }

    void HandleFreeLook()
    {
      if (Input.IsMouseButtonPressed(MouseButton.Right))
      {
        _lastMousePosition = new Vector3(Input.MousePosition());
        _isFlying = true;
      }

      if (Input.IsMouseButtonDown(MouseButton.Right))
      {
        Vector3 current = new Vector3(Input.MousePosition());
        Vector3 delta   = _lastMousePosition - current;
        _lastMousePosition = current;

        _yaw   += delta.X * lookSensitivity;
        _pitch  = System.Math.Clamp(_pitch + delta.Y * lookSensitivity, -89.0f, 89.0f);

        _targetRotation = Quaternion.FromEulerAngles(_pitch, _yaw, 0);

        float speed = Input.IsKeyDown(KeyCode.LeftShift)
          ? flySpeed * flySpeedMultiplier
          : flySpeed;

        speed *= Time.DeltaTime;

        if (Input.IsKeyDown(KeyCode.W) || Input.IsKeyDown(KeyCode.Up))
        {
          _targetPosition += Camera.Main.Forward * speed;
        }

        if (Input.IsKeyDown(KeyCode.S) || Input.IsKeyDown(KeyCode.Down))
        {
          _targetPosition -= Camera.Main.Forward * speed;
        }

        if (Input.IsKeyDown(KeyCode.D) || Input.IsKeyDown(KeyCode.Right))
        {
          _targetPosition += Camera.Main.Right * speed;
        }

        if (Input.IsKeyDown(KeyCode.A) || Input.IsKeyDown(KeyCode.Left))
        {
          _targetPosition -= Camera.Main.Right * speed;
        }

        if (Input.IsKeyDown(KeyCode.E))
        {
          _targetPosition += Vector3.Up * speed;
        }

        if (Input.IsKeyDown(KeyCode.Q))
        {
          _targetPosition -= Vector3.Up * speed;
        }

        _focalPoint = _targetPosition + Camera.Main.Forward * _orbitDistance;
      }

      if (!Input.IsMouseButtonDown(MouseButton.Right))
      {
        _isFlying = false;
      }
    }

    void HandleFocus()
    {
      if (Input.IsKeyPressed(KeyCode.F))
      {
        _focalPoint    = Vector3.Zero;
        _orbitDistance = 10.0f;
        UpdateOrbitPosition();
      }
    }

    void UpdateOrbitPosition()
    {
      _targetRotation = Quaternion.FromEulerAngles(_pitch, _yaw, 0);
      Vector3 forward = _targetRotation * Vector3.Forward;
      _targetPosition = _focalPoint - forward * _orbitDistance;
    }

    void ApplyMovement()
    {
      float t = Time.DeltaTime * smoothTime;
      Camera.Main.Position = Vector3.Lerp(Camera.Main.Position, _targetPosition, t);
      Camera.Main.Rotation = Quaternion.Slerp(Camera.Main.Rotation, _targetRotation, t);
    }

  } // class EditorCameraController

} // namespace Demo