using Sbx.Core;
using Sbx.Math;

namespace Demo
{
  
  public class CharacterMovement : Behavior
  {
    
    public float MaxSpeed = 8f;
    public float GroundAcceleration = 40f;
    public float AirAcceleration = 10f;
    public float Gravity = -25f;
    public float JumpImpulse = 10f;
    public float GroundFriction = 12f;

    private Vector3 _velocity;
    private bool _isGrounded;

    private IInputProvider _input;
    private CharacterController _controller;

    public override void OnCreate()
    {
      _input = new KeyboardMouseInputProvider();
      _controller = GetComponent<CharacterController>();
    }

    public override void OnUpdate()
    {
      var state = _input.GetState();

      UpdateMotor(state);
    }

    private void UpdateMotor(PlayerInputState state)
    {
      // Read last frame's collision results first
      ReadCollisionResults();

      var moveDir = GetCameraRelativeMove(state);

      ApplyMovement(moveDir);
      ApplyJump(state);
      ApplyGravity();
      ApplyFriction(moveDir);

      MoveAndCollide();
    }

    private void ReadCollisionResults()
    {
      _isGrounded = _controller.IsGrounded;

      var flags = _controller.Flags;

      if (flags.HasFlag(CollisionFlags.Below) && _velocity.Y < 0f)
      {
        _velocity.Y = 0f;
      }

      if (flags.HasFlag(CollisionFlags.Above) && _velocity.Y > 0f)
      {
        _velocity.Y = 0f;
      }
    }

    private Vector3 GetCameraRelativeMove(PlayerInputState state)
    {
      var forward = Camera.Main.FlatForward;
      var right   = Camera.Main.FlatRight;

      var move = forward * state.Move.Y + right * state.Move.X;

      if (move.LengthSquared() > 0.001f)
      {
        move = move.Normalized();
      }

      return move;
    }

    private void ApplyMovement(Vector3 desiredDir)
    {
      float accel = _isGrounded ? GroundAcceleration : AirAcceleration;

      _velocity += desiredDir * accel * Time.DeltaTime;

      var horizontal = new Vector3(_velocity.X, 0f, _velocity.Z);

      if (horizontal.Length() > MaxSpeed)
      {
        horizontal = horizontal.Normalized() * MaxSpeed;
        _velocity.X = horizontal.X;
        _velocity.Z = horizontal.Z;
      }
    }

    private void ApplyFriction(Vector3 desiredDir)
    {
      if (!_isGrounded)
      {
        return;
      }

      if (desiredDir.LengthSquared() > 0.001f)
      {
        return;
      }

      var horizontal = new Vector3(_velocity.X, 0f, _velocity.Z);

      float speed = horizontal.Length();

      if (speed <= 0f)
      {
        return;
      }

      float drop = speed * GroundFriction * Time.DeltaTime;

      float newSpeed = MathF.Max(speed - drop, 0f);

      horizontal *= newSpeed / speed;

      _velocity.X = horizontal.X;
      _velocity.Z = horizontal.Z;
    }

    private void ApplyJump(PlayerInputState state)
    {
      if (_isGrounded && state.Jump)
      {
        _velocity.Y = JumpImpulse;
      }
    }

    private void ApplyGravity()
    {
      _velocity.Y += Gravity * Time.DeltaTime;
    }

    private void MoveAndCollide()
    {
      _controller.Move(_velocity * Time.DeltaTime);
    }

  } // class CharacterMovement

} // namespace Demo