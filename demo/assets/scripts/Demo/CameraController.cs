using Sbx.Core;
using Sbx.Math;

namespace Demo
{
  public class CameraController : Behavior
  {

    public float normalSpeed = 0.1f;
    public float fastSpeed = 0.4f;
    private float _movementSpeed;

    public float movementTime = 10.0f;

    private Vector3 _newPosition;

    public float rotationAmount = 1.2f;
    private Quaternion _newRotation;

    public Vector3 zoomAmount = new Vector3(0, -2.0f, -2.0f);
    public Vector3 minZoom = new Vector3(0, 3.0f, 3.0f);
    public Vector3 maxZoom = new Vector3(0, 60.0f, 60.0f);
    private Vector3 _newZoom;

    public float lookSensitivity = 0.2f;
    private float _pitch = 0.0f;
    private float _yaw = 0.0f;
    private bool _isLooking = false;
    private Vector3 _lookStartPosition;

    private Vector3 _dragStartPosition;
    private Vector3 _dragCurrentPosition;

    private Vector3 _rotateStartPosition;
    private Vector3 _rotateCurrentPosition;

    private Transform transform;

    public override void OnCreate()
    {
      transform = GetComponent<Transform>();

      _newPosition = transform.Position;
      _newRotation = transform.Rotation;
      _newZoom = Camera.Main.Position;
    }

    public override void OnUpdate()
    {
      HandleMouseInput();
      HandleMovementInput();
    }

    void HandleMouseInput()
    {
      if (Input.ScrollDelta().Y != 0)
      {
        _newZoom += Input.ScrollDelta().Y * zoomAmount;
        _newZoom = Vector3.Clamp(_newZoom, minZoom, maxZoom);
      }

      // Pan
      if (Input.IsMouseButtonPressed(MouseButton.Left))
      {
        Plane plane = new Plane(Vector3.Up, Vector3.Zero);
        Ray ray = Camera.Main.ScreenPointToRay(Input.MousePosition());

        if (plane.Raycast(ray, out float t))
        {
          _dragStartPosition = ray.GetPoint(t);
        }
      }

      if (Input.IsMouseButtonDown(MouseButton.Left))
      {
        Plane plane = new Plane(Vector3.Up, Vector3.Zero);
        Ray ray = Camera.Main.ScreenPointToRay(Input.MousePosition());

        if (plane.Raycast(ray, out float entry))
        {
          _dragCurrentPosition = ray.GetPoint(entry);
          _newPosition = transform.Position + _dragStartPosition - _dragCurrentPosition;
        }
      }

      // Yaw (horizontal orbit)
      if (Input.IsMouseButtonPressed(MouseButton.Middle))
      {
        _rotateStartPosition = new Vector3(Input.MousePosition());
      }

      if (Input.IsMouseButtonDown(MouseButton.Middle))
      {
        _rotateCurrentPosition = new Vector3(Input.MousePosition());

        Vector3 difference = _rotateStartPosition - _rotateCurrentPosition;
        _rotateStartPosition = _rotateCurrentPosition;

        _yaw += difference.X / 5.0f;
        _newRotation = Quaternion.Euler(Vector3.Up * _yaw);
      }

      // Free look (pitch + yaw) on right mouse button
      if (Input.IsMouseButtonPressed(MouseButton.Right))
      {
        _lookStartPosition = new Vector3(Input.MousePosition());
        _isLooking = true;
      }

      if (Input.IsMouseButtonDown(MouseButton.Right))
      {
        Vector3 currentPosition = new Vector3(Input.MousePosition());
        Vector3 delta = _lookStartPosition - currentPosition;
        _lookStartPosition = currentPosition;

        _yaw   += delta.X * lookSensitivity;
        _pitch  = System.Math.Clamp(_pitch + delta.Y * lookSensitivity, -89.0f, 89.0f);

        _newRotation = Quaternion.Euler(Vector3.Up * _yaw);
        Camera.Main.Rotation = Quaternion.Euler(Vector3.Right * _pitch);
      }

      if (!Input.IsMouseButtonDown(MouseButton.Right))
      {
        _isLooking = false;
      }
    }

    void HandleMovementInput()
    {
      _movementSpeed = Input.IsKeyDown(KeyCode.LeftShift) ? fastSpeed : normalSpeed;

      if (Input.IsKeyDown(KeyCode.W) || Input.IsKeyDown(KeyCode.Up))
      {
        _newPosition += transform.Forward * _movementSpeed;
      }

      if (Input.IsKeyDown(KeyCode.S) || Input.IsKeyDown(KeyCode.Down))
      {
        _newPosition -= transform.Forward * _movementSpeed;
      }

      if (Input.IsKeyDown(KeyCode.D) || Input.IsKeyDown(KeyCode.Right))
      {
        _newPosition += transform.Right * _movementSpeed;
      }

      if (Input.IsKeyDown(KeyCode.A) || Input.IsKeyDown(KeyCode.Left))
      {
        _newPosition -= transform.Right * _movementSpeed;
      }

      if (Input.IsKeyDown(KeyCode.Q))
      {
        _yaw -= rotationAmount;
        _newRotation = Quaternion.Euler(Vector3.Up * _yaw);
      }

      if (Input.IsKeyDown(KeyCode.E))
      {
        _yaw += rotationAmount;
        _newRotation = Quaternion.Euler(Vector3.Up * _yaw);
      }

      transform.Position = Vector3.Lerp(transform.Position, _newPosition, Time.DeltaTime * movementTime);
      transform.Rotation = Quaternion.Slerp(transform.Rotation, _newRotation, Time.DeltaTime * movementTime);
      Camera.Main.Position = Vector3.Lerp(Camera.Main.Position, _newZoom, Time.DeltaTime * movementTime);
    }

  } // class CameraController

} // namespace Demo
