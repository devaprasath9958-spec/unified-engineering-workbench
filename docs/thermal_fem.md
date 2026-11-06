# 1D Steady-State Thermal Finite Element Method (FEM) Solver

## 1. Problem Description
This module implements a steady-state thermal analysis of heat conduction along a uniform cylindrical or rectangular rod with uniform volumetric heat generation and prescribed temperatures at both boundaries.

The objective is to compute the temperature distribution along the rod using two-node linear finite elements, evaluate convergence under mesh refinement, and verify numerical results against the exact analytical solution.

---

## 2. Governing Differential Equation
The steady-state 1D heat conduction equation with internal volumetric heat generation is given by:

$$-k \frac{d^2 T}{dx^2} = q, \quad x \in [0, L]$$

Rearranging into standard form:

$$\frac{d^2 T}{dx^2} = -\frac{q}{k}$$

Integrating twice with respect to $x$ and applying boundary conditions $T(0) = T_0$ and $T(L) = T_L$, the exact analytical solution is:

$$T_{\text{exact}}(x) = T_0 - \frac{T_0 - T_L}{L} x + \frac{q}{2k} x (L - x)$$

For the specified rod parameters, this simplifies to the exact quadratic polynomial:

$$T_{\text{exact}}(x) = 100 + 420x - 500x^2$$

---

## 3. Physical Parameters and Units

| Parameter | Symbol | Value | Unit | Description |
| :--- | :---: | ---: | :---: | :--- |
| **Rod Length** | $L$ | $1.0$ | $\text{m}$ | Total spatial length of the rod |
| **Cross-Sectional Area** | $A$ | $1.0 \times 10^{-4}$ | $\text{m}^2$ | Uniform cross-sectional area |
| **Thermal Conductivity** | $k$ | $10.0$ | $\text{W/(m}\cdot\text{K)}$ | Material thermal conductivity |
| **Volumetric Heat Generation** | $q$ | $10000.0$ | $\text{W/m}^3$ | Constant internal heat source |
| **Left Boundary Temperature** | $T(0)$ | $100.0$ | $^\circ\text{C}$ | Prescribed Dirichlet temperature at $x = 0$ |
| **Right Boundary Temperature** | $T(L)$ | $20.0$ | $^\circ\text{C}$ | Prescribed Dirichlet temperature at $x = L$ |

---

## 4. Finite Element Formulation

### 4.1 Two-Node Linear Elements
The domain $[0, L]$ is partitioned into $N$ uniform elements of length:

$$h = \frac{L}{N}$$

For element $e$ connecting nodes $x_1$ and $x_2$:
- **Element Stiffness (Conductance) Matrix:**
  $$\mathbf{K}^{(e)} = \frac{k A}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$$

- **Element Heat Load Vector:**
  $$\mathbf{F}^{(e)} = \frac{q A h}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$$

### 4.2 System Assembly & Boundary Conditions
The global system dimension is $(N+1) \times (N+1)$ for $N+1$ nodes:
- Each interior node $i \in [1, N-1]$ receives load contributions from two adjacent elements, yielding $F_i = q A h$ and diagonal entries $K_{i,i} = 2 \frac{k A}{h}$.
- Essential Dirichlet boundary conditions $T(0) = 100^\circ\text{C}$ and $T(L) = 20^\circ\text{C}$ are enforced by modifying the right-hand side of adjacent interior equations:
  $$F'_1 = F_1 + \frac{k A}{h} T(0)$$
  $$F'_{N-1} = F_{N-1} + \frac{k A}{h} T(L)$$
- The resulting tridiagonal, symmetric positive-definite system of $N-1$ unknowns is solved using the **Thomas Algorithm (TDMA)** in $O(N)$ operations with zero external linear algebra dependencies.

---

## 5. Mesh Sizes and Midpoint Error Calculation

Mesh densities evaluated:
$$N \in \{10, 20, 40, 80\} \text{ elements}$$

### Midpoint Evaluation
Between nodes, the finite element solution is interpolated using linear shape functions:

$$T_{\text{FE, mid}} = \frac{T_i + T_{i+1}}{2}$$

The exact analytical temperature at the element midpoint $x_{\text{mid}} = (i + 0.5) h$ is:

$$T_{\text{exact, mid}} = T_{\text{exact}}(x_{\text{mid}})$$

The midpoint absolute error is:

$$\text{error}_e = \left| T_{\text{exact, mid}} - T_{\text{FE, mid}} \right|$$

Theoretical maximum error across all elements:

$$E_{\max}(N) = \frac{q}{8k} h^2 = \frac{10000}{8 \cdot 10} h^2 = 125.0 \cdot h^2 = \frac{125.0}{N^2}$$

### Theoretical & Measured Convergence Results

| Elements ($N$) | Element Length ($h$) | Theoretical Max Midpoint Error | Error Reduction Ratio | Convergence Rate |
| :---: | :---: | :---: | :---: | :---: |
| **10** | $0.1000\ \text{m}$ | $1.250000^\circ\text{C}$ | Baseline | $O(h^2)$ |
| **20** | $0.0500\ \text{m}$ | $0.312500^\circ\text{C}$ | $4.0\times$ | $O(h^2)$ |
| **40** | $0.0250\ \text{m}$ | $0.078125^\circ\text{C}$ | $4.0\times$ | $O(h^2)$ |
| **80** | $0.0125\ \text{m}$ | $0.019531^\circ\text{C}$ | $4.0\times$ | $O(h^2)$ |

Every doubling of $N$ reduces midpoint error by exactly a factor of 4, demonstrating quadratic convergence.

---

## 6. Assumptions and Limitations
1. **Steady-State Conduction:** Transient time-dependent temperature variations are not modeled.
2. **One-Dimensional Heat Flow:** Heat conduction is assumed to occur purely along the axial $x$-direction; radial and circumferential temperature gradients are neglected ($Bi \ll 0.1$).
3. **Constant Thermophysical Properties:** Thermal conductivity $k$ and cross-sectional area $A$ are assumed constant along the rod length.
4. **Boundary Condition Constraints:** The solver currently assumes Dirichlet temperature conditions at both rod ends.

---

## 7. Implementation & Testing Insight
- **Nodal Exactness vs. Midpoint Error:**
  During testing, an important numerical property was observed: for 1D linear finite elements with constant heat generation $q$, the finite element solution at all mesh **nodes** matches the exact analytical solution to floating-point precision ($\approx 10^{-13}$). This occurs because the second central difference operator on a uniform grid has zero truncation error for quadratic polynomials.
  Evaluating errors only at the nodes would misleadingly suggest zero error for any mesh ($N=10$ or $N=80$). The true discretization error is piecewise linear interpolation error, which is maximized at element midpoints. Evaluating errors at element midpoints verified the theoretical quadratic convergence rate $O(h^2) = 125/N^2$.
