using Sbx.Core;
using Sbx.Math;

namespace Demo {

  public class StrategyCameraController : Behavior
  {
    // Pan
    public float keyPanSpeed = 40.0f;
    public float mousePanSpeed = 0.5f;
    public float edgeScrollSpeed = 30.0f;
    public float edgeScrollMargin = 20.0f;

    // Zoom
    public float scrollZoomSpeed = 4.0f;
    public float minHeight = 8.0f;
    public float maxHeight = 250.0f;

    // Rotation
    public float rotationSpeed = 0.3f;

    // Pitch range (negative = looking down; steeper when zoomed in, flatter when zoomed out)
    public float minPitch = -70.0f;
    public float maxPitch = -30.0f;

    // Smoothing
    public float smoothTime = 10.0f;

    // Map bounds (world units from center)
    public float mapHalfWidth = 1024.0f;
    public float mapHalfHeight = 1024.0f;

    private float _yaw = 0.0f;
    private float _pitch = -50.0f;
    private float _targetHeight;

    private Vector3 _focalPoint = Vector3.Zero;
    private Vector3 _targetFocalPoint = Vector3.Zero;
    private Vector3 _targetPosition;
    private Quaternion _targetRotation;

    private Vector3 _lastMousePosition;
    private bool _isPanning = false;

    public override void OnCreate()
    {
      _targetHeight = System.Math.Clamp(Camera.Main.Position.Y, minHeight, maxHeight);
      _focalPoint = Camera.Main.Position + Camera.Main.Forward * _targetHeight;
      _focalPoint = new Vector3(_focalPoint.X, 0.0f, _focalPoint.Z);
      _targetFocalPoint = _focalPoint;

      Vector3 euler = Camera.Main.Rotation.EulerAngles();
      _yaw = euler.Y;
      _pitch = System.Math.Clamp(euler.X, minPitch, maxPitch);

      UpdateCameraPosition();

      _targetPosition = Camera.Main.Position;
      _targetRotation = Camera.Main.Rotation;
    }

    public override void OnUpdate()
    {
      HandleKeyboardPan();
      HandleEdgeScroll();
      // HandleMousePan();
      HandleZoom();
      HandleRotation();

      ClampFocalPoint();
      UpdateCameraPosition();
      ApplyMovement();
    }

    void HandleKeyboardPan()
    {
      float speed = keyPanSpeed * Time.DeltaTime;

      // Scale pan speed with zoom level
      speed *= (_targetHeight / maxHeight) * 2.0f + 0.5f;

      Vector3 forward = FlatForward();
      Vector3 right = FlatRight();

      if (Input.IsKeyDown(KeyCode.W) || Input.IsKeyDown(KeyCode.Up))
      {
        _targetFocalPoint += forward * speed;
      }

      if (Input.IsKeyDown(KeyCode.S) || Input.IsKeyDown(KeyCode.Down))
      {
        _targetFocalPoint -= forward * speed;
      }

      if (Input.IsKeyDown(KeyCode.D) || Input.IsKeyDown(KeyCode.Right))
      {
        _targetFocalPoint += right * speed;
      }

      if (Input.IsKeyDown(KeyCode.A) || Input.IsKeyDown(KeyCode.Left))
      {
        _targetFocalPoint -= right * speed;
      }
    }

    void HandleEdgeScroll()
    {
      // Skip edge scrolling while right-click rotating or middle-click panning
      if (Input.IsMouseButtonDown(MouseButton.Right) || Input.IsMouseButtonDown(MouseButton.Middle))
      {
        return;
      }

      Vector2 mouse = Input.MousePosition();
      Vector2 viewport = Camera.Main.Viewport;

      float speed = edgeScrollSpeed * Time.DeltaTime;
      speed *= (_targetHeight / maxHeight) * 2.0f + 0.5f;

      Vector3 forward = FlatForward();
      Vector3 right = FlatRight();

      if (mouse.X < edgeScrollMargin)
      {
        _targetFocalPoint -= right * speed;
      }
      else if (mouse.X > viewport.X - edgeScrollMargin)
      {
        _targetFocalPoint += right * speed;
      }

      if (mouse.Y < edgeScrollMargin)
      {
        _targetFocalPoint += forward * speed;
      }
      else if (mouse.Y > viewport.Y - edgeScrollMargin)
      {
        _targetFocalPoint -= forward * speed;
      }
    }

    void HandleMousePan()
    {
      if (Input.IsMouseButtonPressed(MouseButton.Middle))
      {
        _lastMousePosition = new Vector3(Input.MousePosition());
        _isPanning = true;
      }

      if (_isPanning && Input.IsMouseButtonDown(MouseButton.Middle))
      {
        Vector3 current = new Vector3(Input.MousePosition());
        Vector3 delta = _lastMousePosition - current;
        _lastMousePosition = current;

        float scale = _targetHeight * 0.001f * mousePanSpeed;

        Vector3 forward = FlatForward();
        Vector3 right = FlatRight();

        _targetFocalPoint += (right * delta.X + forward * -delta.Y) * scale;
      }

      if (!Input.IsMouseButtonDown(MouseButton.Middle))
      {
        _isPanning = false;
      }
    }

    void HandleZoom()
    {
      float scroll = Input.ScrollDelta().Y;

      if (scroll == 0)
      {
        return;
      }

      float zoomAmount = scroll * scrollZoomSpeed * (_targetHeight * 0.1f);
      _targetHeight = System.Math.Clamp(_targetHeight - zoomAmount, minHeight, maxHeight);

      // Adjust pitch based on zoom: steeper when close, flatter when far
      float zoomFactor = (_targetHeight - minHeight) / (maxHeight - minHeight);
      _pitch = System.Math.Clamp(minPitch + zoomFactor * (maxPitch - minPitch), minPitch, maxPitch);
    }

    void HandleRotation()
    {
      // if (Input.IsMouseButtonPressed(MouseButton.Right))
      // {
      //   _lastMousePosition = new Vector3(Input.MousePosition());
      // }

      // if (Input.IsMouseButtonDown(MouseButton.Right))
      // {
      //   Vector3 current = new Vector3(Input.MousePosition());
      //   Vector3 delta = _lastMousePosition - current;
      //   _lastMousePosition = current;

      //   _yaw += delta.X * rotationSpeed;
      // }

      // Q/E rotation
      if (Input.IsKeyDown(KeyCode.Q))
      {
        _yaw -= 60.0f * Time.DeltaTime;
      }

      if (Input.IsKeyDown(KeyCode.E))
      {
        _yaw += 60.0f * Time.DeltaTime;
      }
    }

    void ClampFocalPoint()
    {
      float clampedX = System.Math.Clamp(_targetFocalPoint.X, -mapHalfWidth, mapHalfWidth);
      float clampedZ = System.Math.Clamp(_targetFocalPoint.Z, -mapHalfHeight, mapHalfHeight);

      _targetFocalPoint = new Vector3(clampedX, 0.0f, clampedZ);
    }

    void UpdateCameraPosition()
    {
      _targetRotation = Quaternion.FromEulerAngles(_pitch, _yaw, 0.0f);

      Vector3 back = _targetRotation * -Vector3.Forward;
      _targetPosition = _targetFocalPoint + back * _targetHeight;
    }

    void ApplyMovement()
    {
      float t = Time.DeltaTime * smoothTime;

      _focalPoint = Vector3.Lerp(_focalPoint, _targetFocalPoint, t);
      Camera.Main.Position = Vector3.Lerp(Camera.Main.Position, _targetPosition, t);
      Camera.Main.Rotation = Quaternion.Slerp(Camera.Main.Rotation, _targetRotation, t);
    }

    Vector3 FlatForward()
    {
      Quaternion flat = Quaternion.FromEulerAngles(0.0f, _yaw, 0.0f);
      return flat * Vector3.Forward;
    }

    Vector3 FlatRight()
    {
      Quaternion flat = Quaternion.FromEulerAngles(0.0f, _yaw, 0.0f);
      return flat * Vector3.Right;
    }

  } // class StrategyCameraController

} // namespace Demo
