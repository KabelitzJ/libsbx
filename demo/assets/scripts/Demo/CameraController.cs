using Sbx.Core;
using Sbx.Math;

namespace Demo
{
  public class CameraController : Behavior
  {
    public float moveSpeed = 10f;
    public float fastMultiplier = 3f;

    public float minTilt = 2f;
    public float maxTilt = 89f;

    public float minZoom = 3f;
    public float maxZoom = 100f;

    public float orbitAngle = 90f;

    private LinearSmoothValue tilt;
    private ProportionalSmoothValue zoom;

    private Vector3 target = Vector3.Zero;
    public Vector3 mapCenter = Vector3.Zero;
    public float maxDistance = 100.0f;

    private Vector2 lastMouse;
    private bool dragging = false;

    private Transform transform;

    public override void OnCreate()
    {
      transform = GetComponent<Transform>();

      tilt = new LinearSmoothValue(30f);
      zoom = new ProportionalSmoothValue(30f);
    }

    public override void OnUpdate(float dt)
    {
      HandleKeyboardMovement(dt);
      HandleMouseOrbit(dt);
      HandleKeyRotation(dt);
      HandleZoom(dt);

      tilt.Update(dt, 90f);
      zoom.Update(dt, 10f);
      target.Y = 2.0f;

      UpdateCameraPosition();
    }

    private void HandleKeyboardMovement(float dt)
    {
      Vector3 flatForward = Vector3.Cross(Vector3.Up, transform.Right).Normalized();
      Vector3 flatRight = Vector3.Cross(Vector3.Up, transform.Forward).Normalized();
      Vector3 movement = Vector3.Zero;

      if (Input.IsKeyDown(KeyCode.W))
      {
        movement += flatForward;
      }

      if (Input.IsKeyDown(KeyCode.S))
      {
        movement -= flatForward;
      }

      if (Input.IsKeyDown(KeyCode.A))
      {
        movement += flatRight;
      }

      if (Input.IsKeyDown(KeyCode.D))
      {
        movement -= flatRight;
      }

      float speed = moveSpeed;

      if (Input.IsKeyDown(KeyCode.LeftShift))
      {
        speed = moveSpeed * fastMultiplier;
      }

      if (movement.LengthSquared() > 0f)
      {
        target += movement.Normalized() * speed * dt;

        float dist = Vector3.Distance(target, mapCenter);

        if (dist > maxDistance)
        {
          Vector3 dir = (target - mapCenter).Normalized();
          target = mapCenter + dir * maxDistance;
        }
      }
    }

    private void HandleMouseOrbit(float dt)
    {
      if (Input.IsMouseButtonPressed(MouseButton.Middle))
      {
        dragging = true;
        lastMouse = Input.MousePosition();
      }

      if (Input.IsMouseButtonReleased(MouseButton.Middle))
      {
        dragging = false;
      }

      if (dragging == false)
      {
        return;
      }

      Vector2 mousePos = Input.MousePosition();
      Vector2 delta = mousePos - lastMouse;
      lastMouse = mousePos;

      orbitAngle += delta.X * 80f * dt;

      float newTilt = tilt.Target + delta.Y * 80f * dt;
      newTilt = Math.Clamp(newTilt, minTilt, maxTilt);
      tilt.Set(newTilt);
    }

    private void HandleKeyRotation(float dt)
    {
      if (Input.IsKeyDown(KeyCode.E))
      {
        orbitAngle -= 45f * dt;
      }

      if (Input.IsKeyDown(KeyCode.Q))
      {
        orbitAngle += 45f * dt;
      }
    }

    private void HandleZoom(float dt)
    {
      float scroll = Input.ScrollDelta().Y;

      if (MathF.Abs(scroll) < 0.001f)
      {
        return;
      }

      const float baseSpeed = 1.0f;
      const float exponent = 0.25f;

      float current = zoom.Target;
      float step = baseSpeed * MathF.Pow(current, exponent);
      float offset = scroll * step;

      float newZoom = Math.Clamp(current - offset, minZoom, maxZoom);
      zoom.Set(newZoom);
    }

    private void UpdateCameraPosition()
    {
      float tiltRad = Angle.ToRadians(tilt.Value());
      float orbitRad = Angle.ToRadians(orbitAngle);

      float radius = MathF.Cos(tiltRad) * zoom.Value();
      float height = MathF.Sin(tiltRad) * zoom.Value();

      float x = MathF.Cos(orbitRad) * radius;
      float z = MathF.Sin(orbitRad) * radius;

      Vector3 pos = target + new Vector3(x, height, z);
      transform.Position = pos;
      transform.LookAt(target);
    }

  } // class CameraController

} // namespace Demo
