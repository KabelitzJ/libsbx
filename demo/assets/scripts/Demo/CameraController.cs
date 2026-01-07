using System.Data.Common;
using Sbx.Core;
using Sbx.Math;

namespace Demo
{
  public class CameraController : Behavior
  {

    public float normalSpeed = 0.5f;
    public float fastSpeed = 3.0f;
    private float _movementSpeed;

    public float movementTime = 5.0f;

    private Vector3 _newPosition;

    public float rotationAmount = 1.0f;
    private Quaternion _newRotation;

    public Vector3 zoomAmount = new Vector3(0, -10.0f, 10.0f);
    private Vector3 _newZoom;

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
      _newZoom = Camera.Position;
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
        Logger.Info("{0}", _newZoom);
      }

      if (Input.IsMouseButtonPressed(MouseButton.Left))
      {
        Plane plane = new Plane(Vector3.Up, Vector3.Zero);

        Ray ray = Camera.ScreenPointToRay(Input.MousePosition());

        if (plane.Raycast(ray, out float entry))
        {
          _dragStartPosition = ray.GetPoint(entry);
        }
      }

      if (Input.IsMouseButtonDown(MouseButton.Left))
      {
        Plane plane = new Plane(Vector3.Up, Vector3.Zero);

        Ray ray = Camera.ScreenPointToRay(Input.MousePosition());

        if (plane.Raycast(ray, out float entry))
        {
          _dragCurrentPosition = ray.GetPoint(entry);

          _newPosition = transform.Position + _dragStartPosition - _dragCurrentPosition;
        }
      }

      if (Input.IsMouseButtonPressed(MouseButton.Middle))
      {
        _rotateStartPosition = new Vector3(Input.MousePosition());
      }

      if (Input.IsMouseButtonDown(MouseButton.Middle))
      {
        _rotateCurrentPosition = new Vector3(Input.MousePosition());

        Vector3 difference = _rotateStartPosition - _rotateCurrentPosition;

        _rotateStartPosition = _rotateCurrentPosition;

        _newRotation *= Quaternion.Euler(Vector3.Up * difference.X / 5f);
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
        _newRotation *= Quaternion.Euler(Vector3.Up * rotationAmount);
      }

      if (Input.IsKeyDown(KeyCode.E))
      {
        _newRotation *= Quaternion.Euler(Vector3.Down * rotationAmount);
      }

      if (Input.IsKeyDown(KeyCode.R))
      {
        _newZoom += zoomAmount;
      }

      if (Input.IsKeyDown(KeyCode.F))
      {
        _newZoom -= zoomAmount;
      }

      transform.Position = Vector3.Lerp(transform.Position, _newPosition, Time.DeltaTime * movementTime);
      transform.Rotation = Quaternion.Slerp(transform.Rotation, _newRotation, Time.DeltaTime * movementTime);
      Camera.Position = Vector3.Lerp(Camera.Position, _newZoom, Time.DeltaTime * movementTime);
    }

  } // class CameraController

} // namespace Demo
