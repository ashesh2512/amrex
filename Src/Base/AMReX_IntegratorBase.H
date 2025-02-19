#ifndef AMREX_INTEGRATOR_BASE_H
#define AMREX_INTEGRATOR_BASE_H
#include <AMReX_Config.H>
#include <AMReX_REAL.H>
#include <AMReX_Vector.H>
#include <AMReX_MultiFab.H>

#if defined(AMREX_PARTICLES)
#include <AMReX_Particles.H>
#endif

#include <functional>
#include <type_traits>

namespace amrex {

template<class T, typename Tv = void> struct IntegratorOps;

#if defined(AMREX_PARTICLES)
template<class T>
struct IntegratorOps<T, typename std::enable_if<std::is_base_of<amrex::ParticleContainerBase, T>::value>::type>
{

    static void CreateLike (amrex::Vector<std::unique_ptr<T> >& V, const T& Other)
    {
        // Emplace a new T in V with the same size as Other and get a reference
        V.emplace_back(std::make_unique<T>(Other.Geom(0), Other.ParticleDistributionMap(0), Other.ParticleBoxArray(0)));
        T& pc = *V[V.size()-1];

        // We want the particles to have all the same position, cpu, etc.
        // as in Other, so do a copy from Other to our new particle container.
        Copy(pc, Other);
    }

    static void Copy (T& Y, const T& Other)
    {
        // Copy the contents of Other into Y
        const bool local = true;
        Y.copyParticles(Other, local);
    }

    static void Saxpy (T& Y, const amrex::Real a, T& X)
    {
        // Calculate Y += a * X using a particle-level saxpy function supplied by the particle container T
        typedef amrex::ParIter<T::NStructReal, T::NStructInt, T::NArrayReal, T::NArrayInt> TParIter;
        typedef amrex::Particle<T::NStructReal, T::NStructInt> ParticleType;

        int lev = 0;
        TParIter pty(Y, lev);
        TParIter ptx(X, lev);

        auto checkValid = [&]() -> bool {
            bool pty_v = pty.isValid();
            bool ptx_v = ptx.isValid();
            AMREX_ASSERT(pty_v == ptx_v);
            return pty_v && ptx_v;
        };

        auto ptIncrement = [&](){ ++pty; ++ptx; };

#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (; checkValid(); ptIncrement())
        {
            const int npy  = pty.numParticles();
            const int npx  = ptx.numParticles();
            AMREX_ASSERT(npy == npx);

            ParticleType* psy = &(pty.GetArrayOfStructs()[0]);
            ParticleType* psx = &(ptx.GetArrayOfStructs()[0]);

            auto particle_apply_rhs = T::particle_apply_rhs;

            amrex::ParallelFor ( npy, [=] AMREX_GPU_DEVICE (int i) {
                ParticleType& py = psy[i];
                const ParticleType& px = psx[i];
                particle_apply_rhs(py, a, px);
            });
        }
    }

};
#endif

template<class T>
struct IntegratorOps<T, typename std::enable_if<std::is_same<amrex::Vector<amrex::MultiFab>, T>::value>::type>
{

    static void CreateLike (amrex::Vector<std::unique_ptr<T> >& V, const T& Other, bool Grow = false)
    {
        // Emplace a new T in V with the same size as Other
        V.emplace_back(std::make_unique<T>());
        for (auto const& other_mf : Other) {
            IntVect nGrow = Grow ? other_mf.nGrowVect() : IntVect(0);
            V.back()->push_back(amrex::MultiFab(other_mf.boxArray(), other_mf.DistributionMap(), other_mf.nComp(), nGrow));
        }
    }

    static void Copy (T& Y, const T& Other, bool Grow = true)
    {
        // Copy the contents of Other into Y
        const int size = Y.size();
        for (int i = 0; i < size; ++i) {
            IntVect nGrow = Grow ? Other[i].nGrowVect() : IntVect(0);
            amrex::MultiFab::Copy(Y[i], Other[i], 0, 0, Other[i].nComp(), nGrow);
        }
    }

    static void Saxpy (T& Y, const amrex::Real a, const T& X, bool Grow = false)
    {
        // Calculate Y += a * X
        const int size = Y.size();
        for (int i = 0; i < size; ++i) {
            IntVect nGrow = Grow ? X[i].nGrowVect() : IntVect(0);
            amrex::MultiFab::Saxpy(Y[i], a, X[i], 0, 0, X[i].nComp(), nGrow);
        }
    }

};

template<class T>
struct IntegratorOps<T, typename std::enable_if<std::is_same<amrex::MultiFab, T>::value>::type>
{

    static void CreateLike (amrex::Vector<std::unique_ptr<T> >& V, const T& Other, bool Grow = false)
    {
        // Emplace a new T in V with the same size as Other
        IntVect nGrow = Grow ? Other.nGrowVect() : IntVect(0);
        V.emplace_back(std::make_unique<T>(Other.boxArray(), Other.DistributionMap(), Other.nComp(), nGrow));
    }

    static void Copy (T& Y, const T& Other, bool Grow = true)
    {
        // Copy the contents of Other into Y
        IntVect nGrow = Grow ? Other.nGrowVect() : IntVect(0);
        amrex::MultiFab::Copy(Y, Other, 0, 0, Other.nComp(), nGrow);
    }

    static void Saxpy (T& Y, const amrex::Real a, const T& X, bool Grow = false)
    {
        // Calculate Y += a * X
        IntVect nGrow = Grow ? X.nGrowVect() : IntVect(0);
        amrex::MultiFab::Saxpy(Y, a, X, 0, 0, X.nComp(), nGrow);
    }

};

template<class T>
class IntegratorBase
{
private:
    std::function<void(T&, const T&, const amrex::Real)> Fun;

protected:
    amrex::Real timestep;
    std::function<void (T&, amrex::Real)> post_update;

public:
    IntegratorBase () {}

    IntegratorBase (const T& S_data) {}

    virtual ~IntegratorBase () {}

    virtual void initialize (const T& S_data) = 0;

    void set_rhs (std::function<void(T&, const T&, const amrex::Real)> F)
    {
        Fun = F;
    }

    void set_post_update (std::function<void (T&, amrex::Real)> F)
    {
        post_update = F;
    }

    std::function<void(T&, const T&, const amrex::Real)> get_rhs ()
    {
        return Fun;
    }

    std::function<void (T&, amrex::Real)> get_post_update ()
    {
        return post_update;
    }

    void rhs (T& S_rhs, const T& S_data, const amrex::Real time)
    {
        Fun(S_rhs, S_data, time);
    }

    virtual amrex::Real advance (T& S_old, T& S_new, amrex::Real time, const amrex::Real dt) = 0;

    virtual void time_interpolate (const T& S_new, const T& S_old, amrex::Real timestep_fraction, T& data) = 0;

    virtual void map_data (std::function<void(T&)> Map) = 0;
};

}

#endif
