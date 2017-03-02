/**
 *  @file BlendGasPhase.cpp
 *   ThermoPhase object for the Peng-Robinson gas equation of
 * state - workhorse for %Cantera (see \ref thermoprops
 * and class \link Cantera::BlendGasPhase BlendGasPhase\endlink).
 */

#include "cantera/thermo/BlendGasPhase.h"
#include "cantera/base/vec_functions.h"

using namespace std;

namespace Cantera
{

BlendGasPhase::BlendGasPhase() : m_p0(-1.0),
                                 m_logc0(0.0),
                                 m_bFactor(1.0)
{
}

BlendGasPhase::BlendGasPhase(const std::string& inputFile, const std::string& id_) : m_p0(-1.0),
                                                                                     m_logc0(0.0),
                                                                                     m_bFactor(1.0)
{
    initThermoFile(inputFile, id_);
}

BlendGasPhase::BlendGasPhase(XML_Node& phaseRef, const std::string& id_) : m_p0(-1.0),
                                                                           m_logc0(0.0),
                                                                           m_bFactor(1.0)
{
    initThermoXML(phaseRef, id_);
}

BlendGasPhase::BlendGasPhase(const BlendGasPhase& right) : m_p0(right.m_p0),
                                                           m_logc0(right.m_logc0),
                                                           m_bFactor(right.m_bFactor)
{
    /*
     * Use the assignment operator to do the brunt
     * of the work for the copy constructor.
     */
    *this = right;
}

BlendGasPhase& BlendGasPhase::operator=(const BlendGasPhase& right)
{
    if (&right != this) {
        ThermoPhase::operator=(right);
        m_p0 = right.m_p0;
        m_logc0 = right.m_logc0;
        m_h0_RT = right.m_h0_RT;
        m_cp0_R = right.m_cp0_R;
        m_g0_RT = right.m_g0_RT;
        m_s0_R = right.m_s0_R;
        m_expg0_RT = right.m_expg0_RT;
        m_pp = right.m_pp;
    }
    return *this;
}

ThermoPhase* BlendGasPhase::duplMyselfAsThermoPhase() const
{
    return new BlendGasPhase(*this);
}

doublereal BlendGasPhase::pressure() const
{
    //return (GasConstant * temperature()) / (molarVolume() - Bm) - Am / (pow(molarVolume(), 2) + 2.0 * molarVolume() * Bm - pow(Bm, 2));
    return GasConstant * m_density_I / meanMolecularWeight() * temperature();
}

void BlendGasPhase::setPressure(doublereal p)
{
    //SetRealFluidConstants(); // only depends on mass fractions
    //SetRealFluidThermodynamics(); // also depends on temperature except for Bm
    _updateThermoRealFluid();
    //setDensity(meanMolecularWeight() / GetVolumeFromPressureTemperature(p, temperature()));
    doublereal density_PR = meanMolecularWeight() / GetVolumeFromPressureTemperature(p, temperature());
    doublereal density_I = p * meanMolecularWeight() / (GasConstant * temperature());
    setDensity((1 - m_bFactor) * density_I + m_bFactor * density_PR);
    m_density_I = density_I;
}

// Molar Thermodynamic Properties of the Solution ------------------

doublereal BlendGasPhase::enthalpy_mole() const
{
    _updateThermoRealFluid();
    double h0 = GasConstant * temperature() * mean_X(enthalpy_RT_ref());
    double departure = -GasConstant * temperature() + K1 * (Am - temperature() * dAmdT) + pressure() * molarVolume();
    //printf("h0 = %f, departure = %f\n", h0, departure);
    //printf("rt = %f, v = %f, pv - rt = %f\n", GasConstant * temperature(), molarVolume(), pressure() * molarVolume() - GasConstant * temperature());
    //return h0 + departure;
    return h0 + m_bFactor * departure;
}

doublereal BlendGasPhase::entropy_mole() const
{
    // this is still the ideal gas value
    return GasConstant * (mean_X(entropy_R_ref()) - sum_xlogx() - std::log(pressure() / m_spthermo->refPressure()));
}

doublereal BlendGasPhase::cp_mole() const
{
    _updateThermoRealFluid();
    doublereal cp0 = GasConstant * mean_X(cp_R_ref()); // ideal gas part
    doublereal departure = -GasConstant - K1 * temperature() * d2AmdT2 - temperature() * pow(dPdT, 2) / dPdV; // departure function
    //    printf("cp0 = %f, departure = %f\n", cp0, departure);
    //    printf("dPdT = %f, dPdV = %f, d2AmdT2 = %f, K1 = %f\n", dPdT, dPdV, d2AmdT2, K1);
    //return cp0;
    //return cp0 + departure;
    return cp0 + m_bFactor * departure;
}

doublereal BlendGasPhase::cv_mole() const
{
    _updateThermoRealFluid();
    doublereal cv0 = GasConstant * mean_X(cp_R_ref()) - GasConstant; // ideal gas part
    doublereal departure = (-temperature() * d2AmdT2 * K1); // departure function
    //return cv0 + departure;
    return cv0 + m_bFactor * departure;
}

doublereal BlendGasPhase::cv_tr(doublereal atomicity) const
{
    warn_deprecated("BlendGasPhase::cv_tr", "To be removed after Cantera 2.2.");
    // k is the species number
    int dum = 0;
    int type = m_spthermo->reportType();
    doublereal c[12];
    doublereal minTemp_;
    doublereal maxTemp_;
    doublereal refPressure_;

    if (type != 111) {
        throw CanteraError("Error in BlendGasPhase.cpp", "cv_tr only supported for StatMech!. \n\n");
    }

    m_spthermo->reportParams(dum, type, c, minTemp_, maxTemp_, refPressure_);

    // see reportParameters for specific details
    return c[3];
}

doublereal BlendGasPhase::cv_trans() const
{
    warn_deprecated("BlendGasPhase::cv_trans", "To be removed after Cantera 2.2.");
    return 1.5 * GasConstant;
}

doublereal BlendGasPhase::cv_rot(double atom) const
{
    warn_deprecated("BlendGasPhase::cv_rot", "To be removed after Cantera 2.2.");
    return std::max(cv_tr(atom) - cv_trans(), 0.);
}

doublereal BlendGasPhase::cv_vib(const int k, const doublereal T) const
{
    warn_deprecated("BlendGasPhase::cv_vib", "To be removed after Cantera 2.2.");
    // k is the species number
    int dum = 0;
    int type = m_spthermo->reportType();
    doublereal c[12];
    doublereal minTemp_;
    doublereal maxTemp_;
    doublereal refPressure_;

    c[0] = temperature();

    // basic sanity check
    if (type != 111) {
        throw CanteraError("Error in BlendGasPhase.cpp", "cv_vib only supported for StatMech!. \n\n");
    }

    m_spthermo->reportParams(dum, type, c, minTemp_, maxTemp_, refPressure_);

    // see reportParameters for specific details
    return c[4];
}

doublereal BlendGasPhase::standardConcentration(size_t k) const
{
    return pressure() / (GasConstant * temperature());
}

void BlendGasPhase::getActivityCoefficients(doublereal* ac) const
{
    for (size_t k = 0; k < m_kk; k++) {
        ac[k] = 1.0;
    }
}

void BlendGasPhase::getStandardChemPotentials(doublereal* muStar) const
{
    const vector_fp& gibbsrt = gibbs_RT_ref();
    scale(gibbsrt.begin(), gibbsrt.end(), muStar, _RT());
    double tmp = log(pressure() / m_spthermo->refPressure());
    tmp *= GasConstant * temperature();
    for (size_t k = 0; k < m_kk; k++) {
        muStar[k] += tmp; // add RT*ln(P/P_0)
    }
}

//  Partial Molar Properties of the Solution --------------

void BlendGasPhase::getChemPotentials(doublereal* mu) const
{
    getStandardChemPotentials(mu);
    doublereal rt = temperature() * GasConstant;
    for (size_t k = 0; k < m_kk; k++) {
        double xx = std::max(SmallNumber, moleFraction(k));
        mu[k] += rt * (log(xx));
    }
}

void BlendGasPhase::getPartialMolarEnthalpies(doublereal* hbar) const
{
//    const vector_fp& _h = enthalpy_RT_ref();
//    doublereal rt = GasConstant * temperature();
//    scale(_h.begin(), _h.end(), hbar, rt);

    _updateThermoRealFluid();
    const vector_fp& _h = enthalpy_RT_ref();
    doublereal rt = GasConstant * temperature();
    vector_fp hbar0(m_kk);
    scale(_h.begin(), _h.end(), &hbar0[0], rt);
    doublereal temp = Am - temperature() * dAmdT;
    for (size_t k = 0; k < m_kk; k++) {
        //hbar[k] = hbar0[k] - rt + dK1dN[k] * temp + K1 * (dAmdN[k] - temperature() * d2AmdTdN[k]) + pressure() * dVdN[k];
        hbar[k] = hbar0[k] + m_bFactor * (-rt + dK1dN[k] * temp + K1 * (dAmdN[k] - temperature() * d2AmdTdN[k]) + pressure() * dVdN[k]);
        //hbar[k] = hbar0[k];
        //printf("T = %.5g, k = %d, X = %.5g, hbar = %.5g, hbar0 = %.5g, departure = %.5g\n", temperature(), k, moleFraction(k), hbar[k], hbar0[k], hbar[k]-hbar0[k]);
        //printf("rt = %f, dvdn = %f, pv - rt = %f\n", rt, dVdN[k], pressure() * dVdN[k] - rt);
    }
}

void BlendGasPhase::getPartialMolarEntropies(doublereal* sbar) const
{
    const vector_fp& _s = entropy_R_ref();
    scale(_s.begin(), _s.end(), sbar, GasConstant);
    doublereal logp = log(pressure() / m_spthermo->refPressure());
    for (size_t k = 0; k < m_kk; k++) {
        doublereal xx = std::max(SmallNumber, moleFraction(k));
        sbar[k] += GasConstant * (-logp - log(xx));
    }
}

void BlendGasPhase::getPartialMolarIntEnergies(doublereal* ubar) const
{
    const vector_fp& _h = enthalpy_RT_ref();
    doublereal rt = GasConstant * temperature();
    for (size_t k = 0; k < m_kk; k++) {
        ubar[k] = rt * (_h[k] - 1.0);
    }
}

void BlendGasPhase::getPartialMolarCp(doublereal* cpbar) const
{
    const vector_fp& _cp = cp_R_ref();
    scale(_cp.begin(), _cp.end(), cpbar, GasConstant);
}

void BlendGasPhase::getPartialMolarVolumes(doublereal* vbar) const
{
    double vol = 1.0 / molarDensity();
    for (size_t k = 0; k < m_kk; k++) {
        vbar[k] = vol;
    }
}

// Properties of the Standard State of the Species in the Solution --

void BlendGasPhase::getEnthalpy_RT(doublereal* hrt) const
{
    const vector_fp& _h = enthalpy_RT_ref();
    copy(_h.begin(), _h.end(), hrt);
}

void BlendGasPhase::getEntropy_R(doublereal* sr) const
{
    const vector_fp& _s = entropy_R_ref();
    copy(_s.begin(), _s.end(), sr);
    double tmp = log(pressure() / m_spthermo->refPressure());
    for (size_t k = 0; k < m_kk; k++) {
        sr[k] -= tmp;
    }
}

void BlendGasPhase::getGibbs_RT(doublereal* grt) const
{
    const vector_fp& gibbsrt = gibbs_RT_ref();
    copy(gibbsrt.begin(), gibbsrt.end(), grt);
    double tmp = log(pressure() / m_spthermo->refPressure());
    for (size_t k = 0; k < m_kk; k++) {
        grt[k] += tmp;
    }
}

void BlendGasPhase::getPureGibbs(doublereal* gpure) const
{
    const vector_fp& gibbsrt = gibbs_RT_ref();
    scale(gibbsrt.begin(), gibbsrt.end(), gpure, _RT());
    double tmp = log(pressure() / m_spthermo->refPressure());
    tmp *= _RT();
    for (size_t k = 0; k < m_kk; k++) {
        gpure[k] += tmp;
    }
}

void BlendGasPhase::getIntEnergy_RT(doublereal* urt) const
{
    const vector_fp& _h = enthalpy_RT_ref();
    for (size_t k = 0; k < m_kk; k++) {
        urt[k] = _h[k] - 1.0;
    }
}

void BlendGasPhase::getCp_R(doublereal* cpr) const
{
    const vector_fp& _cpr = cp_R_ref();
    copy(_cpr.begin(), _cpr.end(), cpr);
}

void BlendGasPhase::getStandardVolumes(doublereal* vol) const
{
    double tmp = 1.0 / molarDensity();
    for (size_t k = 0; k < m_kk; k++) {
        vol[k] = tmp;
    }
}

// Thermodynamic Values for the Species Reference States ---------

void BlendGasPhase::getEnthalpy_RT_ref(doublereal* hrt) const
{
    const vector_fp& _h = enthalpy_RT_ref();
    copy(_h.begin(), _h.end(), hrt);
}

void BlendGasPhase::getGibbs_RT_ref(doublereal* grt) const
{
    const vector_fp& gibbsrt = gibbs_RT_ref();
    copy(gibbsrt.begin(), gibbsrt.end(), grt);
}

void BlendGasPhase::getGibbs_ref(doublereal* g) const
{
    const vector_fp& gibbsrt = gibbs_RT_ref();
    scale(gibbsrt.begin(), gibbsrt.end(), g, _RT());
}

void BlendGasPhase::getEntropy_R_ref(doublereal* er) const
{
    const vector_fp& _s = entropy_R_ref();
    copy(_s.begin(), _s.end(), er);
}

void BlendGasPhase::getIntEnergy_RT_ref(doublereal* urt) const
{
    const vector_fp& _h = enthalpy_RT_ref();
    for (size_t k = 0; k < m_kk; k++) {
        urt[k] = _h[k] - 1.0;
    }
}

void BlendGasPhase::getCp_R_ref(doublereal* cprt) const
{
    const vector_fp& _cpr = cp_R_ref();
    copy(_cpr.begin(), _cpr.end(), cprt);
}

void BlendGasPhase::getStandardVolumes_ref(doublereal* vol) const
{
    doublereal tmp = _RT() / m_p0;
    for (size_t k = 0; k < m_kk; k++) {
        vol[k] = tmp;
    }
}

void BlendGasPhase::initThermo()
{
    ThermoPhase::initThermo();
    m_p0 = refPressure();
    m_h0_RT.resize(m_kk);
    m_g0_RT.resize(m_kk);
    m_expg0_RT.resize(m_kk);
    m_cp0_R.resize(m_kk);
    m_s0_R.resize(m_kk);
    m_pp.resize(m_kk);

    // ctitical properties init
    Tcrit.resize(m_kk);
    Pcrit.resize(m_kk);
    rhocrit.resize(m_kk);
    Vcrit.resize(m_kk);
    Zcrit.resize(m_kk);
    omega.resize(m_kk);
    sigma.resize(m_kk);
    dipole.resize(m_kk);

    // read in critical properties (currently hard-coded)
    ReadCriticalProperties();

    Tcrit_IJ.resize(m_kk*m_kk);
    Pcrit_IJ.resize(m_kk*m_kk);
    Vcrit_IJ.resize(m_kk*m_kk);
    Zcrit_IJ.resize(m_kk*m_kk);
    omega_IJ.resize(m_kk*m_kk);

    cst_a.resize(m_kk*m_kk);
    cst_b.resize(m_kk);
    cst_c.resize(m_kk*m_kk);

    dPdN.resize(m_kk);
    dVdN.resize(m_kk);
    dAmdN.resize(m_kk);
    d2AmdTdN.resize(m_kk);
    dK1dN.resize(m_kk);
}

void BlendGasPhase::ReadCriticalProperties() const
{
    //! \brief Reads critical properties based on JP's papaer
    for (size_t k = 0; k < m_kk; k++) {
        if (speciesName(k) == "H2") {
            Tcrit[k] = 33.0; // K
            Pcrit[k] = 1.284e+6; // Pa
            Vcrit[k] = 64.28e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = -0.216;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "O2") {
            Tcrit[k] = 154.5800; // K
            Pcrit[k] = 5.0430e+6; // Pa
            Vcrit[k] = 73.37e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0222;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "H2O") {
            Tcrit[k] = 647.10; // K
            Pcrit[k] = 22.064e+6; // Pa
            Vcrit[k] = 55.95e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.3443;
            sigma[k] = 0.0;
            dipole[k] = 1.855;
        } else if (speciesName(k) == "O") {
            Tcrit[k] = 105.28; // K
            Pcrit[k] = 7.088e+6; // Pa
            Vcrit[k] = 41.21e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "H") {
            Tcrit[k] = 190.82; // K
            Pcrit[k] = 31.013e+6; // Pa
            Vcrit[k] = 17.07e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "OH") {
            Tcrit[k] = 105.28; // K
            Pcrit[k] = 7.088e+6; // Pa
            Vcrit[k] = 41.21e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "H2O2") {
            Tcrit[k] = 141.34; // K
            Pcrit[k] = 4.786e+6; // Pa
            Vcrit[k] = 81.93e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "HO2") {
            Tcrit[k] = 141.34; // K
            Pcrit[k] = 4.786e+6; // Pa
            Vcrit[k] = 81.93e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else if (speciesName(k) == "N2") {
            Tcrit[k] = 126.19; // K
            Pcrit[k] = 3.3958e+6; // Pa
            Vcrit[k] = 89.41e-3; // m3/kmol
            rhocrit[k] = molecularWeight(k) / Vcrit[k]; // kg/m3
            Zcrit[k] = (Pcrit[k] * Vcrit[k]) / (GasConstant * Tcrit[k]);
            omega[k] = 0.0372;
            sigma[k] = 0.0;
            dipole[k] = 0.0;
        } else {
            cout << " WARNING -> Unknown species : " << speciesName(k) << ". No critical properties found." << endl;
        }
    }
}

void BlendGasPhase::SetRealFluidConstants() const
{
    //! \brief Assign arrays of critical point data
    for (size_t k = 0; k < m_kk; k++) {
        for (size_t l = 0; l < m_kk; l++) {
            int apos = k * m_kk + l;

            // binary interation parameter
            double tmp_k = 0.1;
            if (k == l) tmp_k = 0.0;

            Tcrit_IJ[apos] = sqrt(Tcrit[l] * Tcrit[k]) * (1.0 - tmp_k);
            Vcrit_IJ[apos] = pow(pow(Vcrit[l], 1.0 / 3.0) + pow(Vcrit[k], 1.0 / 3.0), 3.0) / 8.0;
            Zcrit_IJ[apos] = 0.5 * (Zcrit[l] + Zcrit[k]);
            Pcrit_IJ[apos] = Zcrit_IJ[apos] * GasConstant * Tcrit_IJ[apos] / Vcrit_IJ[apos];
            omega_IJ[apos] = 0.5 * (omega[l] + omega[k]);
        }
    }

    //! \brief Assign constants used for computing real fluid effects
    for (size_t k = 0; k < m_kk; k++) {
        cst_b[k] = 0.077796 * GasConstant * Tcrit[k] / Pcrit[k];
        for (size_t l = 0; l < m_kk; l++) {
            int apos = k * m_kk + l;
            cst_a[apos] = 0.457236 * pow(GasConstant * Tcrit_IJ[apos], 2.0) / Pcrit_IJ[apos];
            cst_c[apos] = 0.37464 + 1.54226 * omega_IJ[apos] - 0.26992 * pow(omega_IJ[apos], 2);
        }
    }

    // compute Bm
    Bm = 0.0;
    for (size_t k = 0; k < m_kk; k++) {
        Bm += moleFraction(k) * cst_b[k];
    }
}

void BlendGasPhase::SetRealFluidThermodynamics() const
{
    //! \brief Computes the thermodynamic properties of the mixture given a molar volume and temperature
    Am = 0.0;
    dAmdT = 0.0;
    d2AmdT2 = 0.0;
    double temp = molarVolume() * molarVolume() + 2.0 * Bm * molarVolume() - Bm * Bm;
    for (size_t k = 0; k < m_kk; k++) {
        dAmdN[k] = 0.0;
        d2AmdTdN[k] = 0.0;
        for (size_t l = 0; l < m_kk; l++) {
            int apos = k * m_kk + l;

            double X_X = moleFraction(l) * moleFraction(k);
            double A_IJ = cst_a[apos] * pow(1.0 + cst_c[apos] * (1.0 - sqrt(temperature() / Tcrit_IJ[apos])), 2);
            double G = cst_c[apos] * sqrt(temperature() / Tcrit_IJ[apos]) / (1.0 + cst_c[apos] * (1.0 - sqrt(temperature() / Tcrit_IJ[apos])));
            double D = cst_c[apos] * (1.0 + cst_c[apos]) * Tcrit_IJ[apos] / Pcrit_IJ[apos] * sqrt(Tcrit_IJ[apos] / temperature());

            Am += X_X * A_IJ;
            dAmdT -= X_X * A_IJ * G;
            d2AmdT2 += X_X * D;

            dAmdN[k] += moleFraction(l) * A_IJ;
            d2AmdTdN[k] += moleFraction(l) * A_IJ * G;
        }
        dAmdN[k] *= 2.0;
        //printf("Am = %f, dAmdN = %f\n", Am, dAmdN[k]);
        d2AmdTdN[k] *= -2.0 / temperature();
        //printf("dAmdT = %f, d2AmdTdN = %f\n", dAmdT/temperature(), d2AmdTdN[k]);
        dPdN[k] = GasConstant * temperature() / (molarVolume() - Bm) + GasConstant * temperature() * cst_b[k] / pow((molarVolume() - Bm), 2)
            - dAmdN[k] / temp + 2.0 * Am * cst_b[k] * (molarVolume() - Bm) / pow(temp, 2);
        //printf("dPdN = %f, first = %f\n", dPdN[k], GasConstant * temperature() / (molarVolume() - Bm) + GasConstant * temperature() * cst_b[k] / pow((molarVolume() - Bm), 2));
    }
    dAmdT /= temperature();
    d2AmdT2 *= 0.457236 * pow(GasConstant, 2) / (2.0 * temperature());
    dPdT = GasConstant / (molarVolume() - Bm) - dAmdT / (pow(molarVolume(), 2) + 2.0 * molarVolume() * Bm - pow(Bm, 2));
    double arg = GasConstant * temperature() * (molarVolume() + Bm) * pow(((molarVolume()) / (molarVolume() - Bm) + Bm / (molarVolume() + Bm)), 2);
    dPdV = -GasConstant * temperature() / pow((molarVolume() - Bm), 2) * (1.0 - 2.0 * Am / arg);
    //expansivity = -(dPdT) / (molarVolume() * dPdV); //ideal gas: equal to 3.34E-3 (1/K)
    K1 = 1.0 / (sqrt(8.0) * Bm) * log((molarVolume() + (1 - sqrt(2.0)) * Bm) / (molarVolume() + (1 + sqrt(2.0)) * Bm));

    for (size_t k = 0; k < m_kk; k++) {
        dVdN[k] = -dPdN[k] / dPdV;
        dK1dN[k] = 1.0 / temp * dVdN[k] - cst_b[k] / Bm * (K1 + molarVolume() / temp);
        //printf("dVdN = %f, v = %f\n", dVdN[k], molarVolume());
        //printf("dK1dN = %f, K1 = %f\n", dK1dN[k], K1);
    }
}

doublereal BlendGasPhase::GetVolumeFromPressureTemperature(doublereal p_in, doublereal T_in) const
{
    // Computation for Z directly
    doublereal Amat = Am * p_in / pow(GasConstant * T_in, 2);
    doublereal Bmat = Bm * p_in / (GasConstant * T_in);

    doublereal coef_v0 = (pow(Bmat, 3) + pow(Bmat, 2) - Amat * Bmat);
    doublereal coef_v1 = (-3.0 * pow(Bmat, 2) - 2.0 * Bmat + Amat);
    doublereal coef_v2 = (Bmat - 1.0);
    return GasConstant * T_in * (GetCubicRoots(coef_v0, coef_v1, coef_v2)) / p_in;
}

doublereal BlendGasPhase::GetCubicRoots(doublereal a0, doublereal a1, doublereal a2) const
{
    //! \brief Solves the cubic root equation given three parameters
    const double eps = 1.0e-12;
    double p, q, u, v, Det;
    double arg, signArg;
    double phi;

    double Z  = 0.0;
    double Z1 = 0.0;
    double Z2 = 0.0;
    double Z3 = 0.0;

    p = (3.0 * a1 - pow(a2, 2)) / 3.0;
    q = a0 + 2 * pow(a2, 3) / 27.0 - a2 * a1 / 3.0;

    Det = pow(p / 3.0, 3) + pow(q / 2.0, 2);

    if (Det > 0) { // only one real root
        arg     = -q/2.0 + sqrt(Det); 
        signArg = (arg>0)?1.0:-1.0; 
        u       = signArg*pow(fabs(arg), 1.0/3.0);
        arg     = -q/2.0 - sqrt(Det); 
        signArg = (arg>0)?1.0:-1.0; 
        v       = signArg*pow(fabs(arg), 1.0/3.0);
        Z       = -a2/3.0 + u + v;
        // printf ("single root %e of  %e %e %e \n",Z, a0, a1,a2);
        //printf("Z+= %f\n", Z);
    } else if (fabs(Det) <= eps) {
        printf ("double root \n");
        arg     = -q/2.0;
        signArg = (arg>0)?1.0:-1.0;
        u       = signArg*pow(fabs(arg), 1.0/3.0);
        arg     = -q/2.0;
        signArg = (arg>0)?1.0:-1.0;
        v       = signArg*pow(fabs(arg), 1.0/3.0);
        Z       = -a2/3.0 + u + v;
    } else {
        arg = -q / (2.0 * sqrt(pow(fabs(p) / 3.0, 3.0)));
        phi = acos(arg);

        Z1 = -a2 / 3.0 + 2.0 * sqrt(fabs(p) / 3.0) * cos((phi) / 3.0);
        Z2 = -a2 / 3.0 - 2.0 * sqrt(fabs(p) / 3.0) * cos((phi - Pi) / 3.0);
        Z3 = -a2 / 3.0 - 2.0 * sqrt(fabs(p) / 3.0) * cos((phi + Pi) / 3.0);

        //jph For fully consistent results we should impose a fugacity constraint
        Z = min(min(Z1, Z2), Z3);
        if (Z < 0) {
            Z = max(max(Z1, Z2), Z3);
        }
    }
    // double sanity = pow(Z,3)+pow(Z,2)*a2+Z*a1+a0;
    //  printf("Sanity = %e\n ", sanity);
    return Z;
}

void BlendGasPhase::setToEquilState(const doublereal* mu_RT)
{
    const vector_fp& grt = gibbs_RT_ref();

    /*
     * Within the method, we protect against inf results if the
     * exponent is too high.
     *
     * If it is too low, we set
     * the partial pressure to zero. This capability is needed
     * by the elemental potential method.
     */
    doublereal pres = 0.0;
    for (size_t k = 0; k < m_kk; k++) {
        double tmp = -grt[k] + mu_RT[k];
        if (tmp < -600.) {
            m_pp[k] = 0.0;
        } else if (tmp > 300.0) {
            double tmp2 = tmp / 300.;
            tmp2 *= tmp2;
            m_pp[k] = m_p0 * exp(300.) * tmp2;
        } else {
            m_pp[k] = m_p0 * exp(tmp);
        }
        pres += m_pp[k];
    }
    // set state
    setState_PX(pres, &m_pp[0]);
}

doublereal BlendGasPhase::critTemperature() const
{
    return mean_X(Tcrit);
}

doublereal BlendGasPhase::critPressure() const
{
    return mean_X(Pcrit);
}

doublereal BlendGasPhase::critVolume() const
{
    return mean_X(Vcrit);
}

void BlendGasPhase::getCritTemperature(doublereal* const tc) const
{
    copy(Tcrit.begin(), Tcrit.end(), tc);
}

void BlendGasPhase::getCritPressure(doublereal* const pc) const
{
    copy(Pcrit.begin(), Pcrit.end(), pc);
}

void BlendGasPhase::getCritVolume(doublereal* const vc) const
{
    copy(Vcrit.begin(), Vcrit.end(), vc);
}

void BlendGasPhase::getCritCompressibility(doublereal* const zc) const
{
    copy(Zcrit.begin(), Zcrit.end(), zc);
}

void BlendGasPhase::getAcentricFactor(doublereal* const om) const
{
    copy(omega.begin(), omega.end(), om);
}

void BlendGasPhase::getDipoleMoment(doublereal* const di) const
{
    copy(dipole.begin(), dipole.end(), di);
}

doublereal BlendGasPhase::critCompressibility() const
{
    return mean_X(Zcrit);
}

void BlendGasPhase::_updateThermo() const
{
    static const int cacheId = m_cache.getId();
    CachedScalar cached = m_cache.getScalar(cacheId);
    doublereal tnow = temperature();

    // If the temperature has changed since the last time these
    // properties were computed, recompute them.
    if (cached.state1 != tnow) {
        m_spthermo->update(tnow, &m_cp0_R[0], &m_h0_RT[0], &m_s0_R[0]);
        cached.state1 = tnow;

        // update the species Gibbs functions
        for (size_t k = 0; k < m_kk; k++) {
            m_g0_RT[k] = m_h0_RT[k] - m_s0_R[k];
        }
        m_logc0 = log(m_p0 / (GasConstant * tnow));
    }

}

void BlendGasPhase::_updateThermoRealFluid() const
{
    static const int cacheId = m_cache.getId();
    CachedArray cached = m_cache.getArray(cacheId);

    doublereal tnow = temperature();
    doublereal rhonow = density();
    vector_fp ynow(m_kk);
    getMassFractions(DATA_PTR(ynow));

    if (cached.value != ynow) {
        // update everything since mass fractions have changed
        SetRealFluidConstants();
        SetRealFluidThermodynamics();
        cached.value = ynow;
        cached.state1 = tnow;
        cached.state2 = rhonow;
    } else if (cached.state1 != tnow || cached.state2 != rhonow) {
        // only update thermodynamics sing only temperature/density has changed
        SetRealFluidThermodynamics();
        cached.state1 = tnow;
        cached.state2 = rhonow;
    }
}

}