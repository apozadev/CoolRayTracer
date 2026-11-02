# Okay → Cool → Cooler → SuperCool

*A talk about low-level optimization, with a small ray tracer as the test subject.*

**The talk in one sentence:** the same program got 3.3× faster on one core and 4.1× faster on sixteen, without changing its algorithm or a single byte of its output, by changing how its data is laid out and how it uses SIMD.

**Running time:** about 40 minutes plus questions. Section 8 (threads) is the easiest cut.

## Before the talk

- Build the `x64-release` preset.
- Run `.\bench.ps1 -Runs 3` (one thread, about a minute and a half) and `.\bench.ps1 -Runs 3 -Threads 0` (all cores, a few seconds). The numbers in this script were measured on an AMD Ryzen 7 1700 (8 cores, 16 threads, first-generation Zen). Re-run them if you present on another machine.
- Have these files open: [OkayRayTracer.cpp](OkayRayTracer/OkayRayTracer.cpp), [CoolRayTracer.cpp](CoolRayTracer/CoolRayTracer.cpp), [CoolerRayTracer.cpp](CoolerRayTracer/CoolerRayTracer.cpp), [SimdVec4.h](Util/SimdVec4.h), [SuperCoolRayTracer.cpp](SuperCoolRayTracer/SuperCoolRayTracer.cpp) and [win32_main.cpp](Platform/win32_main.cpp).
- Plain paragraphs are what you say. **On screen** lines are what to show.

---

## 1. Cold open (~2 min)

**On screen:** `output.bmp`.

This is a picture of some spheres. It's the only picture in this talk, so enjoy it while it lasts.

Four different programs drew it, and they all produce exactly the same file, byte for byte. Same algorithm, same formulas, same random numbers. On one core, the slowest takes almost ten seconds. The fastest takes three.

This talk is about where those seven seconds went. There's no clever new algorithm involved. We only change where the data sits in memory and how much of it the CPU handles per instruction. And one of the "improvements" made things slower, which might be the most useful part of the talk.

**Demo (optional):** prove the four images are identical.

```powershell
Get-FileHash out\build\x64-release\*RayTracer\output.bmp
```

---

## 2. The loop we're optimizing (~3 min)

I promise this isn't a graphics talk. You need exactly one sentence about ray tracing: for every pixel we shoot a few rays into the scene, and for each ray we ask "which of these 81 objects do you hit first?" There are 80 spheres and a floor. When a ray hits something, it bounces and we ask again, up to five times.

Take the vocabulary away and the hot loop looks like this:

```text
for each query                      // ~7.4 million camera rays, plus their bounces
    best = none
    for each of the 81 objects
        if the query hits this object closer than best
            best = this object
    do some work with best          // pick a material, bounce
```

It's a linear scan with a running minimum. If you've ever looped over every entity, every order or every record to find the best match, you've written this loop.

The image is 1280 × 720 pixels with 8 rays per pixel, so about 7.4 million rays. Times 81 objects, that's roughly 600 million tests before a single ray has bounced. At that volume, every extra pointer, call and byte in the inner loop shows up on the clock.

**Rules of the game**

- **The output must not change.** We hash the image, and if the hash changes, the result doesn't count.
- **Only the layout and the SIMD differ.** Everything else is shared: the scene, the arithmetic shortcuts, the random number generator and the threading code. When two versions run at different speeds, the layout and the SIMD are why.
- **Runs alternate.** `bench.ps1` switches program on every run, so thermal drift and background noise hit all of them equally. We report the median of three runs, on one thread and on all sixteen.

---

## 3. Four things a CPU likes (~2 min)

Here's a lens for the rest of the talk. A modern CPU is fast when it gets:

1. **Data that sits together.** Reading the next bytes of an array is cheap. Following a pointer to find out where to read next is a chain of waits.
2. **Code it can see through.** The compiler can only optimize across calls it can inline, and the branch predictor loves boring, repetitive patterns.
3. **Independent work.** A core runs several instructions at once when they don't depend on each other, and one SIMD instruction can apply the same operation to 4 or 8 values.
4. **Nothing to share.** When two cores write to the same 64-byte cache line, the line ping-pongs between them.

We'll check each version against these four.

---

## 4. Okay: the textbook version (~7 min)

**On screen:** [OkayRayTracer.cpp](OkayRayTracer/OkayRayTracer.cpp), first the class declarations, then `HittableList::Hit` and `Sphere::Hit`.

Okay is how most of us learned this: the "Ray Tracing in One Weekend" tutorial. It's a great tutorial, and its code is textbook object-oriented C++:

```cpp
// Condensed from OkayRayTracer.cpp
class Hittable { public: virtual bool Hit(const ray& _oRay, float _fTMax, HitRecord& oRecord_) const = 0; };
class Sphere : public Hittable { /* ... */ point3 vCenter; float fRadiusSqr; std::shared_ptr<Material> pMaterial; };
class HittableList : public Hittable { /* ... */ std::vector<std::shared_ptr<Hittable>> vObjects; };

class Material { public: virtual bool Scatter(/* ... */) const = 0; };  // Lambertian, Metal, Dielectric
struct HitRecord { point3 vPoint; vec3 vNormal; std::shared_ptr<Material> pMaterial; float fT; };
```

Every sphere and every material is its own heap allocation, made with `make_shared`. The list holds `shared_ptr`s. When a ray hits something, `Hit` fills a `HitRecord` with the hit point, the normal and a `shared_ptr` to the material. It reads beautifully. It's... okay.

**What the CPU actually does for one sphere**

1. Load the `shared_ptr` from the vector. That's 16 bytes: a pointer to the object and a pointer to its control block.
2. Follow it and load the object's vtable pointer.
3. Load the address of `Hit` from the vtable.
4. Make an indirect call: set up a stack frame, run, return.
5. Only now load the center and radius and do the twenty or so floating-point operations we actually care about.

The call is a wall for the compiler. It can't inline `Hit`, so it can't hoist anything out of the loop or share work between spheres. Every call starts from scratch.

**Work that gets thrown away**

```cpp
if (fT > 0.001f && fT < _fTMax)
{
  oRecord_.fT = fT;
  oRecord_.vPoint = _oRay.at(fT);
  oRecord_.vNormal = Normalize(oRecord_.vPoint - vCenter);  // square root + division
  oRecord_.pMaterial = pMaterial;                           // atomic increment + decrement
  return true;
}
```

Whenever a sphere is closer than the best so far, it builds the full hit record, normal included. If a later sphere turns out to be even closer, all of that goes in the bin.

**The sneaky one: `shared_ptr`**

Copying a `shared_ptr` isn't just copying a pointer. It increments a reference count, and because that count is thread-safe, the increment is atomic. That means a locked instruction, which on x86 is also a full memory fence. Whatever the record pointed to before gets an atomic decrement. And then `HittableList` copies the whole record again:

```cpp
for (const std::shared_ptr<Hittable>& pObject : vObjects)
{
  if (pObject->Hit(_oRay, fClosestSoFar, oTempRecord))  // virtual call
  {
    bHitAnything = true;
    fClosestSoFar = oTempRecord.fT;
    oRecord_ = oTempRecord;                              // another shared_ptr copy
  }
}
```

So every "closer hit" costs a handful of atomic operations, on top of the call overhead.

**What's *not* the problem: cache misses.** The classic "OOP is slow" story is pointer chasing across a fragmented heap. Here, all 81 objects and their materials take about 10 KB, which stays in the 32 KB L1 cache the whole time. Okay pays for indirection, lost inlining, wasted work and atomics. With a hundred thousand objects, the cache misses would come on top.

**Result:** 9.85 s on one thread. On sixteen threads it takes 1.36 s, a 7.3× speedup, while the other three versions get around 9×. Hold that thought; we'll come back to it.

**Scorecard:**
- Data together: ✗
- Code it can see through: ✗
- Independent work: buried behind calls
- Nothing to share: ✗

---

## 5. Cool: same math, different layout (~6 min)

**On screen:** [CoolRayTracer.cpp](CoolRayTracer/CoolRayTracer.cpp): `struct Hittable`, `struct Scene` and the loop in `UpdateScreenBufferPartial`.

Cool keeps every formula and changes the shape of the data.

```cpp
struct Hittable
{
  HittableType eType;
  union
  {
    Sphere oSphere;
    Plane oPlane;
  };
};

struct Scene
{
  std::vector<Hittable> vHittables;
  std::vector<Material> vMaterials;
};
```

Objects are now values in one array, 20 bytes each, with no pointers. Materials live in a second array with the same index. The hot loop becomes:

```cpp
for (int i = 0; i < g_oScene.vHittables.size(); i++)
{
  float fCandidateT;
  if (HitHittable(oRay, g_oScene.vHittables[i], fCandidateT) && fCandidateT < fClosestT)
  {
    fClosestT = fCandidateT;
    iHittableIdx = i;
  }
}
```

**Why it's faster**

1. **Values, not pointers.** The next sphere is 20 bytes further along the array. There's no chain of dependent loads (vector, object, vtable, function) before the math can start.
2. **A `switch` instead of a virtual call.** The compiler sees both cases, so it can inline the sphere test into the loop and optimize them together. And "80 spheres, then a floor" is about the most predictable branch pattern there is.
3. **Less work in the loop.** The loop only computes the distance. The normal and the material are looked up once, for the winner.
4. **Hot/cold split.** The scan never touches the materials, so they stay out of its way. At this size it barely matters, but in a bigger scene the scan streams 20 bytes per object instead of 40.
5. **Nothing shared.** No reference counts, no atomics, no allocations.

**Result:** 5.50 s on one thread, 1.8× faster than Okay. On sixteen threads it takes 0.62 s, 2.2× faster.

This is the cheapest win in the talk. We wrote no clever code. We just removed the work the CPU was doing in between our formulas.

**Weak spots**

- **One object at a time.** Each test is around twenty scalar operations. The CPU's SIMD units could do 4 or 8 of them at once, and they sit idle.
- **A tag on every object.** Each object carries a type tag that's checked on every iteration. A union is also as big as its biggest member, so with more shape types, small shapes would pay for large ones.
- **The wrong shape for SIMD.** The layout is an "array of structures": everything about one sphere sits together. That's perfect for one at a time, and the wrong shape for eight at a time, which is where we're heading.

**Scorecard:**
- Data together: ✓
- Code it can see through: ✓
- Independent work: only what the core finds on its own
- Nothing to share: ✓

---

## 6. Cooler: the SIMD trap (~6 min)

**On screen:** [SimdVec4.h](Util/SimdVec4.h) (`HorizontalSum` and `Dot`), then `struct Sphere` in [CoolerRayTracer.cpp](CoolerRayTracer/CoolerRayTracer.cpp).

We have SIMD registers that hold four floats. Our vectors have three floats. You can probably see where this is going.

In the hit test, Cooler replaces `vec3` with `SimdVec4`: x, y, z and a zero packed into one SSE register. Subtracting two vectors becomes one instruction instead of three. On paper, that should be a big win.

```cpp
struct Sphere
{
  SimdVec4 vCenter;  // x, y, z, 0 in one __m128
  float fRadiusSqr;
};
```

**Result:** 7.96 s on one thread, 45% *slower* than Cool. This is the plot twist hiding in the names: Cooler is not faster. It's in the lineup because it's the most common first attempt at SIMD, and the most common way that attempt fails.

**Why it lost**

1. **SIMD wants vertical work.** Lane 0 combines with lane 0, lane 1 with lane 1. A dot product is horizontal: you need x + y + z, which live in different lanes of the *same* register. So you shuffle, add, shuffle, add, and each step waits for the one before:

   ```cpp
   inline __m128 HorizontalSum(__m128 _v)
   {
     __m128 vShuf = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(2, 3, 0, 1)); // (y, x, w, z)
     __m128 vSums = _mm_add_ps(_v, vShuf);                           // (x+y, x+y, z+w, z+w)
     vShuf = _mm_movehl_ps(vShuf, vSums);                            // (z+w, z+w, ...)
     return _mm_add_ss(vSums, vShuf);                                // (x+y) + (z+w)
   }
   ```

   The sphere test needs two dot products.
2. **A quarter of every register is a zero.** We start at 75% efficiency, at best.
3. **The scalar code was already parallel.** On x64, float math already runs in SSE registers, one value per register, and an out-of-order core executes the independent x, y and z operations at the same time. Packing them into one register didn't add parallelism, because the hardware already had it. It only added shuffles.
4. **Crossing between worlds costs.** The ray is converted to `SimdVec4` on every bounce, each dot product comes back out as a scalar, and the normal goes back to a `vec3` through a temporary array.
5. **The data got fatter.** `__m128` needs 16-byte alignment, so each object grew from 20 bytes to 48.

**The takeaway:** if your SIMD code is full of shuffles and horizontal adds, you're probably vectorizing along the wrong axis. And the only reason we know Cooler is worse is that we measured it. Reading the code, it looks like a win.

One more detail: even Cooler beats Okay, 7.96 s against 9.85 s, because it kept Cool's layout. The layout fix was worth more than the SIMD mistake cost.

**Scorecard:**
- Independent work: attempted, along the wrong axis
- Data together: worse than Cool (48 bytes per object instead of 20)

---

## 7. SuperCool: turn the data sideways (~8 min)

**On screen:** [SuperCoolRayTracer.cpp](SuperCoolRayTracer/SuperCoolRayTracer.cpp): `struct Scene`, then `HitSphere8`.

The fix for Cooler isn't to give up on SIMD. It's to turn the data sideways.

Instead of putting one sphere's x, y and z in one register, put the x of *eight* spheres in one register, their y in another and their z in a third. That's a *structure of arrays*, one array per field:

```cpp
struct Scene
{
  std::vector<Material> vMaterials;

  // Sphere positions
  std::vector<float> vPosX;
  std::vector<float> vPosY;
  std::vector<float> vPosZ;

  // Sphere radii squared
  std::vector<float> vRadiusSqr;
};
```

Now the same formula is purely vertical. Here's one step side by side:

```cpp
// Cool: one sphere
const vec3 vSphereToRay = _oRay.vOrigin - _oSphere.vCenter;
const float h = Dot(vSphereToRay, _oRay.vDir);

// SuperCool: eight spheres, one instruction per operation
const __m256 ocx = _mm256_sub_ps(rox, cx);
const __m256 ocy = _mm256_sub_ps(roy, cy);
const __m256 ocz = _mm256_sub_ps(roz, cz);
const __m256 h = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(ocx, rdx), _mm256_mul_ps(ocy, rdy)), _mm256_mul_ps(ocz, rdz));
```

Lane *i* only ever talks to lane *i*. There are no shuffles until the very end.

**Techniques worth stealing (none of them are specific to graphics)**

1. **Broadcast the constant side.** The ray is the same for all eight spheres, so it's copied into all eight lanes with `_mm256_set1_ps`. One load per field (`_mm256_loadu_ps`) brings in eight spheres.
2. **Turn branches into masks.** `t = (t0 > 0) ? t0 : ((t1 > 0) ? t1 : -1)` becomes two compares and two blends. Every lane computes both answers and a mask picks one, so there's no branch to mispredict:

   ```cpp
   const __m256 t = _mm256_blendv_ps(_mm256_blendv_ps(_mm256_set1_ps(-1.f), t1, t1ValidMask), t0, t0ValidMask);
   ```
3. **Bail out per group.** `_mm256_movemask_ps` squeezes the 8-lane mask into an integer. If no lane can hit, we return before the square root. For a typical ray, most groups of eight contain nothing it can hit.
4. **Pad instead of handling the tail.** The arrays are padded to a multiple of 8, and a lane mask ignores the padding, so there's no leftover scalar loop.
5. **Special-case the odd one out.** There's exactly one floor, so it's tested once, after the loop. The type switch is gone from the hot loop entirely.

**Result:** 3.00 s on one thread, 1.8× faster than Cool and 3.3× faster than Okay. On sixteen threads it takes 0.33 s, 4.1× faster than Okay.

**Weak spots**

- **Only the scan is SIMD.** Everything after it (the material, the random sampling, the bounce) is still scalar, one ray at a time. The faster the scan gets, the bigger the share of time the rest takes. That's Amdahl's law.
- **The minimum goes through memory.** After every group with a candidate, the eight distances are stored to an array and scanned with a scalar loop.
- **On this CPU, 8-wide isn't quite 8-wide.** First-generation Zen executes each 256-bit AVX instruction as two 128-bit halves, so we get roughly SSE-class throughput. The same code on a newer CPU would likely gain more.
- **Alignment isn't guaranteed.** `std::vector` doesn't promise 32-byte alignment, so some of those loads straddle two cache lines.
- **AVX is assumed.** There's no runtime check, so on a CPU without AVX the program crashes.
- **It's the hardest version to change.** Adding a new shape means a new set of arrays and a new kernel. Okay was the easiest version to extend and SuperCool is the hardest. That trade-off is real, and it's why you do this to the small part of the code that's hot, not everywhere.

**Scorecard:**
- Data together: ✓ (four tight streams)
- Code it can see through: ✓ (no type switch, no per-sphere branches)
- Independent work: ✓✓ (8 lanes)
- Nothing to share: ✓

---

## 8. Interlude: threads (~3 min, optional)

**On screen:** [win32_main.cpp](Platform/win32_main.cpp): `TileQueue` and `RenderWorkerThread`.

All four versions share the same threading code, so it doesn't explain the differences between them, with one exception. But first, three details worth stealing:

```cpp
static constexpr int g_iTileSize = 32;

struct alignas(64) TileQueue
{
  volatile LONG lNextTile;
  char aPadding[64 - sizeof(LONG)];
};
```

1. **The work queue is one counter.** Threads grab 32 × 32-pixel tiles with an atomic increment. The counter sits alone on its own 64-byte cache line. Otherwise, every increment would also knock whatever variable the linker placed next to it out of the other cores' caches. That's *false sharing*: cores fighting over a cache line without actually sharing any data.
2. **Tiles don't share cache lines either.** A tile row is 32 pixels × 4 bytes = 128 bytes, exactly two cache lines. Image rows are 5120 bytes and the buffer is page-aligned, so no two threads ever write to the same line of the image.
3. **The random generator is reseeded per tile,** so the image is identical with one thread or sixteen. That's what makes the hash check possible.

Now, remember Okay scaling worse? Here's the speedup from 16 threads for each version: SuperCool 9.1×, Cool 8.9×, Cooler 8.6×, Okay 7.3×. With 8 physical cores, around 9× is a healthy result, and Okay falls behind.

We went to some trouble to avoid false sharing, and `shared_ptr` brought back the real thing through the back door. Every thread keeps bumping the reference counts of the same few materials; the floor alone fills most of the frame. Each increment needs exclusive ownership of the counter's cache line, so that line ping-pongs between cores. On this Ryzen, the cores sit in two clusters of four, and moving a line between clusters is slower still.

To be honest, that's my leading suspect, not a proven diagnosis. Proving it is an easy experiment, and it's first on the future list.

---

## 9. Recap (~2 min)

**On screen:** this table.

| Version | The idea | 1 thread | 16 threads | vs. Okay (1 / 16 threads) |
|---|---|---|---|---|
| Okay | Classes, virtual calls, heap objects, `shared_ptr` | 9.85 s | 1.36 s | 1.0× / 1.0× |
| Cool | Values in one array, `switch`, less work in the loop | 5.50 s | 0.62 s | 1.8× / 2.2× |
| Cooler | SIMD on each vector (x, y, z in one register) | 7.96 s | 0.92 s | 1.2× / 1.5× |
| SuperCool | Structure of arrays, 8 spheres per instruction | 3.00 s | 0.33 s | 3.3× / 4.1× |

Every version produced the same image, down to the byte. Here's what I'd like you to take away:

1. **Lay data out for how the hot loop reads it,** not for how you'd describe the domain.
2. **Keep the hot loop lean.** Move per-result work and cold data out of it.
3. **Let the compiler see your code.** Virtual calls are walls for the optimizer.
4. **Vectorize across items, not across x, y and z.**
5. **Shared writes don't scale,** including the ones hidden inside library types.
6. **Measure, and check that the output didn't change.** Cooler looked like a win until we timed it.

---

## 10. Future improvements (~3 min)

Roughly ordered from quick wins to big changes:

1. **Confirm the `shared_ptr` suspicion.** Swap it for a raw pointer in Okay's `HitRecord` and re-run the benchmark. If the 16-thread scaling jumps to Cool's level, case closed.
2. **Profile instead of guessing.** A sampling profiler (AMD uProf on this machine, Superluminal, or the Visual Studio profiler) would show how much of SuperCool's time is still in the scalar code, and whether the next step should target the scan or the bounce.
3. **Keep the minimum in registers.** Track a running minimum per lane across all groups and reduce once at the end, instead of storing to an array and scanning it after every group.
4. **Align the arrays, or block them.** Allocate the SoA arrays 32- or 64-byte aligned. Or go a step further with an "array of structures of arrays" (AoSoA): blocks of `x[8] y[8] z[8] r²[8]`, so each group of eight spheres fills exactly two cache lines.
5. **Match the hardware.** On this first-gen Zen, a 4-wide SSE version of the same kernel might be just as fast; on newer CPUs, try AVX2 or AVX-512. Pick the path at startup with `cpuid` instead of assuming AVX.
6. **Let the compiler help.** `/arch:AVX2` plus fused multiply-add (`/fp:contract`) could speed up the scalar parts too. But fused multiply-add rounds differently, so the byte-for-byte check would have to become "the images match within a tolerance." Your correctness bar decides which optimizations you're allowed to take.
7. **Go wide along the other axis.** Process 8 *rays* at once, so the scalar code becomes SIMD too. That needs uniform work, which means batching and sorting rays (for example, by material) so neighbouring lanes do the same thing. Batching and sorting work into uniform chunks is the general idea.
8. **Change the algorithm.** A spatial index (for ray tracers, a bounding volume hierarchy) skips most objects instead of testing them faster. At 81 objects the gain is modest; at 100,000 objects it's everything. And it stacks with everything in this talk rather than replacing it.

**Closing line:** We made it 3.3 times faster on one core and 4.1 times faster on sixteen, and the picture didn't change by a single byte. We never touched the algorithm. The next big jump is the opposite kind of change: not testing the objects you can rule out in advance. That's a different talk.
