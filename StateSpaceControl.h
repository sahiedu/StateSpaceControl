#ifndef STATE_SPACE_CONTROL_H
#define STATE_SPACE_CONTROL_H

#include "Arduino.h"
#include "BasicLinearAlgebra.h"
#include "Model.h"

using namespace BLA;

template <int X, int U, int Y = X>
class StateSpaceController
{
    Matrix<X, X> ALC;
    Matrix<U, Y> N_bar;

   public:
    // System model
    const Model<X, U, Y> &model;

    // Control variables
    Matrix<X> x_hat;  // state estimate
    Matrix<U> u;      // control input
    Matrix<Y> r;      // reference input (assumed to be of the same dimension as the observation y)
    Matrix<U> w_hat;  // estimate of a disturbance / error in the system model (used by the integral controller)

    // Control Gains
    Matrix<U, X> K;  // regulator Gain
    Matrix<X, Y> L;  // estimator gain
    Matrix<U, Y> I;  // integral control gain

    StateSpaceController(const Model<X, U, Y> &_model)
        : model(_model),
          x_hat(Zeros<X>()),
          u(Zeros<U>()),
          r(Zeros<Y>()),
          w_hat(Zeros<U>()),
          L(Zeros<X, Y>()),
          I(Zeros<U, Y>())
    {
    }

    void initialise()
    {
        // If reference tracking is enabled we'll need to precalculate Nbar which maps the reference input to a control
        // input offset
        Matrix<X + Y, X + U> sys = (model.A || model.B) && (model.C || model.D);

        // Find an inverse for the aggregated matrix
        Matrix<X + U, X + Y> sysInv;

        // case 1: more outputs than inputs - find the left inverse
        if (model.inputs < model.outputs)
        {
            sysInv = Inverse(~sys * sys) * ~sys;
        }
        // case 2: more than, or the same number of outputs as inputs - find the right inverse
        else
        {
            sysInv = ~sys * Inverse(sys * ~sys);
        }

        // Split it up and multiply it with K to find NBar
        N_bar = K * sysInv.template Submatrix<X, Y>(0, X) + sysInv.template Submatrix<U, Y>(X, X);

        // If estimation is enabled we can also save a bit of processing by precalculating the expression: A - L * C
        ALC = model.A - L * model.C;
    }

    void update(const Matrix<Y> &y, const float dt = 0.0f)
    {
        // If estimation is enabled, update the state estimate
        x_hat += (ALC * x_hat + model.B * u + L * y) * dt;

        // Calculate the control input required to drive the state to 0.
        u = -K * x_hat;

        // If reference tracking is enabled then offset the control input to drive the state to the reference input r
        u += N_bar * r;

        // If integral control is enabled then windup the control input to offset a (presumably) constant disturbance w
        w_hat += I * (y - r) * dt;
        u += w_hat;
    }
};

template <int X, int U, int Y = X>
class Simulation
{
   public:
    Matrix<X> x;

    const Model<X, U, Y> &model;

    Simulation(const Model<X, U, Y> &_model) : model(_model), x(Zeros<X>()) {}

    Matrix<Y> step(const Matrix<U> &u, const float dt)
    {
        x += (model.A * x + model.B * u) * dt;
        return model.C * x;
    }
};

#endif  // STATE_SPACE_CONTROL_H
