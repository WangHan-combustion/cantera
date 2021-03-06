/**
 *  @file GasKinetics.cpp Homogeneous kinetics in ideal gases
 */

// This file is part of Cantera. See License.txt in the top-level directory or
// at http://www.cantera.org/license.txt for license and copyright information.

#include "cantera/kinetics/GasKinetics.h"
#include "cantera/kinetics/RxnActiveEdt.h"
#include "cantera/numerics/eigen_dense.h"

using namespace std;
using namespace Eigen;

namespace Cantera {
GasKinetics::GasKinetics(thermo_t *thermo)
    : BulkKinetics(thermo), m_logp_ref(0.0), m_logc_ref(0.0),
      m_logStandConc(0.0), m_pres(0.0) {}

void GasKinetics::reduceFrom(const GasKinetics &right,
                             const std::vector<std::uint8_t> &iActiv) {
  assert(iActiv.size() == right.nReactions());

  invalidateCache();
  resizeSpecies();

  m_temp = 0.0;
  m_logp_ref = 0.0;
  m_logc_ref = 0.0;
  m_logStandConc = 0.0;
  m_pres = 0.0;
  m_ROP_ok = false;

  // phase
  m_kk = right.m_kk;
  m_thermo = right.m_thermo; // DANGER -> shallow pointer copy
  m_start = right.m_start;
  m_mindim = right.m_mindim;
  m_rxnphase = right.m_rxnphase;
  m_phaseindex = right.m_phaseindex;
  m_surfphase = right.m_surfphase;
  m_skipUndeclaredSpecies = right.m_skipUndeclaredSpecies;
  m_skipUndeclaredThirdBodies = right.m_skipUndeclaredThirdBodies;

  // reactions
  // prepare _nActive, _idList, and _idMap
  size_t _nActive = 0;
  for (const auto i : iActiv) _nActive += (size_t) i;
  std::vector<size_t> _idList(_nActive);
  if (_nActive > 0) {
    for (size_t i = 0, j = 0; i < iActiv.size(); ++i) {
      if (iActiv[i]) _idList[j++] = i;
    }
  }
  std::vector<size_t> _idMap(iActiv.size(), 0);
  for (size_t i = 1; i < _idMap.size(); ++i)
    _idMap[i] = _idMap[i - 1] + iActiv[i - 1];
  // make rop vectors
  m_rfn = vector_fp(_nActive, 0.0);
  m_rkcn = vector_fp(_nActive, 0.0);
  m_ropf = vector_fp(_nActive, 0.0);
  m_ropr = vector_fp(_nActive, 0.0);
  m_ropnet = vector_fp(_nActive, 0.0);
  m_perturb = vector_fp(_nActive, 1.0);

  // make m_reactions
  RxnActiveEdt::editVecs(m_reactions, right.m_reactions, _idList);
  // make m_reactantStoich, m_revProductStoich, and m_irrevProductStoich
  RxnActiveEdt::editStoichMng(m_reactantStoich, right.m_reactantStoich, iActiv,
                             _idMap);
  RxnActiveEdt::editStoichMng(m_revProductStoich, right.m_revProductStoich,
                             iActiv, _idMap);
  RxnActiveEdt::editStoichMng(m_irrevProductStoich, right.m_irrevProductStoich,
                             iActiv, _idMap);
  // make m_revindex, m_irrev
  RxnActiveEdt::editRevs(m_revindex, m_irrev, right.m_revindex, right.m_irrev,
                        iActiv, _idMap);
  // make m_dn
  RxnActiveEdt::editVecs(m_dn, right.m_dn, _idList);
  // make m_rates
  RxnActiveEdt::editRates(m_rates, right.m_rates, iActiv, _idMap);

  // make PLOG_RXN
  RxnActiveEdt::editRates(m_plog_rates, right.m_plog_rates, iActiv, _idMap);
  // make CHEBYSHEV_RXN
  RxnActiveEdt::editRates(m_cheb_rates, right.m_cheb_rates, iActiv, _idMap);
  // make THREE_BODY_RXN
  RxnActiveEdt::editThirdBody(m_3b_concm, right.m_3b_concm, iActiv, _idMap);
  concm_3b_values.resize(m_3b_concm.workSize());

  // make FALLOFF_RXN/CHEMACT_RXN
  // prepare _nActiveFallOff, _idListFallOff, and _idMapFallOff
  std::vector<std::uint8_t> iActivFallOff(right.m_fallindx.size());
  for (size_t i = 0; i < right.m_fallindx.size(); ++i) {
    iActivFallOff[i] = iActiv[right.m_fallindx[i]];
  }
  size_t _nActiveFallOff = 0;
  for (const auto i : iActivFallOff)
    _nActiveFallOff += i;
  std::vector<size_t> _idListFallOff(_nActiveFallOff);
  if (_nActiveFallOff > 0) {
    for (size_t i = 0, j = 0; i < iActivFallOff.size(); ++i) {
      if (iActivFallOff[i]) _idListFallOff[j++] = i;
    }
  }
  std::vector<size_t> _idMapFallOff(iActivFallOff.size(), 0);
  for (size_t i = 1; i < _idMapFallOff.size(); i++)
    _idMapFallOff[i] = _idMapFallOff[i - 1] + iActivFallOff[i - 1];

  m_rfn_high = vector_fp(_nActiveFallOff, 0.);
  m_rfn_low = vector_fp(_nActiveFallOff, 0.);
  m_fallindx.resize(_nActiveFallOff);
  for (size_t i = 0; i < m_fallindx.size(); i++)
    m_fallindx[i] = _idMap[right.m_fallindx[_idListFallOff[i]]];
  m_rfallindx.clear();
  for (size_t i = 0; i < _nActiveFallOff; i++)
    m_rfallindx[_idMap[right.m_fallindx[_idListFallOff[i]]]] = i;

  RxnActiveEdt::editRates(m_falloff_high_rates, right.m_falloff_high_rates,
                         iActivFallOff, _idMapFallOff);
  RxnActiveEdt::editRates(m_falloff_low_rates, right.m_falloff_low_rates,
                         iActivFallOff, _idMapFallOff);
  RxnActiveEdt::editThirdBody(m_falloff_concm, right.m_falloff_concm,
                             iActivFallOff, _idMapFallOff);
  concm_falloff_values.resize(m_falloff_concm.workSize());
  RxnActiveEdt::editFalloff(m_falloffn, right.m_falloffn, _idListFallOff);
  falloff_work.resize(m_falloffn.workSize());
}

void GasKinetics::update_rates_T()
{
    doublereal T = thermo().temperature();
    doublereal P = thermo().pressure();
    m_logStandConc = log(thermo().standardConcentration());
    doublereal logT = log(T);

    if (T != m_temp) {
        if (!m_rfn.empty()) {
            m_rates.update(T, logT, m_rfn.data());
        }

        if (!m_rfn_low.empty()) {
            m_falloff_low_rates.update(T, logT, m_rfn_low.data());
            m_falloff_high_rates.update(T, logT, m_rfn_high.data());
        }
        if (!falloff_work.empty()) {
            m_falloffn.updateTemp(T, falloff_work.data());
        }
        updateKc();
        m_ROP_ok = false;
    }

    if (T != m_temp || P != m_pres) {
        if (m_plog_rates.nReactions()) {
            m_plog_rates.update(T, logT, m_rfn.data());
            m_ROP_ok = false;
        }

        if (m_cheb_rates.nReactions()) {
            m_cheb_rates.update(T, logT, m_rfn.data());
            m_ROP_ok = false;
        }
    }
    m_pres = P;
    m_temp = T;
}

void GasKinetics::update_rates_C() {
  thermo().getActivityConcentrations(m_conc.data());
  doublereal ctot = thermo().molarDensity();

  // 3-body reactions
  if (!concm_3b_values.empty()) {
    m_3b_concm.update(m_conc, ctot, concm_3b_values.data());
  }

  // Falloff reactions
  if (!concm_falloff_values.empty()) {
    m_falloff_concm.update(m_conc, ctot, concm_falloff_values.data());
  }

  // P-log reactions
  if (m_plog_rates.nReactions()) {
    double logP = log(thermo().pressure());
    m_plog_rates.update_C(&logP);
  }

  // Chebyshev reactions
  if (m_cheb_rates.nReactions()) {
    double log10P = log10(thermo().pressure());
    m_cheb_rates.update_C(&log10P);
  }

  m_ROP_ok = false;
}

void GasKinetics::updateKc() {
  thermo().getStandardChemPotentials(m_grt.data());
  fill(m_rkcn.begin(), m_rkcn.end(), 0.0);

  // compute Delta G^0 for all reversible reactions
  getRevReactionDelta(m_grt.data(), m_rkcn.data());

  doublereal rrt = 1.0 / thermo().RT();
  for (size_t i = 0; i < m_revindex.size(); i++) {
    size_t irxn = m_revindex[i];
    m_rkcn[irxn] = std::min(
        exp(m_rkcn[irxn] * rrt - m_dn[irxn] * m_logStandConc), BigNumber);
  }

  for (size_t i = 0; i != m_irrev.size(); ++i) {
    m_rkcn[m_irrev[i]] = 0.0;
  }
}

void GasKinetics::getEquilibriumConstants(doublereal *kc) {
  update_rates_T();
  thermo().getStandardChemPotentials(m_grt.data());
  fill(m_rkcn.begin(), m_rkcn.end(), 0.0);

  // compute Delta G^0 for all reactions
  getReactionDelta(m_grt.data(), m_rkcn.data());

  doublereal rrt = 1.0 / thermo().RT();
  for (size_t i = 0; i < nReactions(); i++) {
    kc[i] = exp(-m_rkcn[i] * rrt + m_dn[i] * m_logStandConc);
  }

  // force an update of T-dependent properties, so that m_rkcn will
  // be updated before it is used next.
  m_temp = 0.0;
}

void GasKinetics::processFalloffReactions() {
  // use m_ropr for temporary storage of reduced pressure
  vector_fp &pr = m_ropr;

  for (size_t i = 0; i < m_rfn_low.size(); i++) {
    pr[i] = m_rfn_low[i] / (m_rfn_high[i] + SmallNumber);
  }
  m_falloff_concm.multiply(pr.data(), concm_falloff_values.data());
  for (size_t i = 0; i < m_rfn_low.size(); i++) {
    AssertFinite(pr[i], "GasKinetics::processFalloffReactions",
                 "pr[{}] is not finite.", i);
  }

  m_falloffn.pr_to_falloff(pr.data(), falloff_work.data());

  for (size_t i = 0; i < m_rfn_low.size(); i++) {
    if (reactionType(m_fallindx[i]) == FALLOFF_RXN) {
      pr[i] *= m_rfn_high[i];
    } else { // CHEMACT_RXN
      pr[i] *= m_rfn_low[i];
    }
  }

  scatter_copy(pr.begin(), pr.begin() + m_rfn_low.size(), m_ropf.begin(),
               m_fallindx.begin());
}

void GasKinetics::updateROP() {
  update_rates_C();
  update_rates_T();
  if (m_ROP_ok) {
    return;
  }

  // copy rate coefficients into ropf
  m_ropf = m_rfn;
  // multiply ropf by enhanced 3b conc for all 3b rxns
  if (!concm_3b_values.empty()) {
    m_3b_concm.multiply(m_ropf.data(), concm_3b_values.data());
  }
  if (m_falloff_high_rates.nReactions()) {
    processFalloffReactions();
  }
  // multiply by perturbation factor
  // multiply_each(m_ropf.begin(), m_ropf.end(), m_perturb.begin());
  Map<ArrayXd>(m_ropf.data(), m_ropf.size()) *=
      Map<ArrayXd>(m_perturb.data(), m_ropf.size()).array();

  // copy the forward rates to the reverse rates
  m_ropr = m_ropf;

  // for reverse rates computed from thermochemistry, multiply the forward
  // rates copied into m_ropr by the reciprocals of the equilibrium constants
  // multiply_each(m_ropr.begin(), m_ropr.end(), m_rkcn.begin());
  Map<ArrayXd>(m_ropr.data(), m_ropr.size()) *=
      Map<const ArrayXd>(m_rkcn.data(), m_ropf.size()).array();

  // multiply ropf by concentration products
  m_reactantStoich.multiply(m_conc.data(), m_ropf.data());

  // for reversible reactions, multiply ropr by concentration products
  m_revProductStoich.multiply(m_conc.data(), m_ropr.data());

  // for (size_t j = 0; j != nReactions(); ++j) {
  //     m_ropnet[j] = m_ropf[j] - m_ropr[j];
  // }
  Map<VectorXd>(m_ropnet.data(), nReactions()).noalias() =
      Map<const VectorXd>(m_ropf.data(), nReactions()) -
      Map<const VectorXd>(m_ropr.data(), nReactions());

  for (size_t i = 0; i < m_rfn.size(); i++) {
    AssertFinite(m_rfn[i], "GasKinetics::updateROP", "m_rfn[{}] is not finite.",
                 i);
    AssertFinite(m_ropf[i], "GasKinetics::updateROP",
                 "m_ropf[{}] is not finite.", i);
    AssertFinite(m_ropr[i], "GasKinetics::updateROP",
                 "m_ropr[{}] is not finite.", i);
  }
  m_ROP_ok = true;
}

void GasKinetics::getFwdRateConstants(doublereal *kfwd) {
  update_rates_C();
  update_rates_T();

  // copy rate coefficients into ropf
  m_ropf = m_rfn;

  // multiply ropf by enhanced 3b conc for all 3b rxns
  if (!concm_3b_values.empty()) {
    m_3b_concm.multiply(m_ropf.data(), concm_3b_values.data());
  }

  if (m_falloff_high_rates.nReactions()) {
    processFalloffReactions();
  }

  // multiply by perturbation factor
  multiply_each(m_ropf.begin(), m_ropf.end(), m_perturb.begin());

  for (size_t i = 0; i < nReactions(); i++) {
    kfwd[i] = m_ropf[i];
  }
}

bool GasKinetics::addReaction(shared_ptr<Reaction> r) {
  // operations common to all reaction types
  bool added = BulkKinetics::addReaction(r);
  if (!added) {
    return false;
  }

  switch (r->reaction_type) {
  case ELEMENTARY_RXN:
    addElementaryReaction(dynamic_cast<ElementaryReaction &>(*r));
    break;
  case THREE_BODY_RXN:
    addThreeBodyReaction(dynamic_cast<ThreeBodyReaction &>(*r));
    break;
  case FALLOFF_RXN:
  case CHEMACT_RXN:
    addFalloffReaction(dynamic_cast<FalloffReaction &>(*r));
    break;
  case PLOG_RXN:
    addPlogReaction(dynamic_cast<PlogReaction &>(*r));
    break;
  case CHEBYSHEV_RXN:
    addChebyshevReaction(dynamic_cast<ChebyshevReaction &>(*r));
    break;
  default:
    throw CanteraError("GasKinetics::addReaction",
                       "Unknown reaction type specified: {}", r->reaction_type);
  }
  return true;
}

void GasKinetics::addFalloffReaction(FalloffReaction &r) {
  // install high and low rate coeff calculators and extend the high and low
  // rate coeff value vectors
  size_t nfall = m_falloff_high_rates.nReactions();
  m_falloff_high_rates.install(nfall, r.high_rate);
  m_rfn_high.push_back(0.0);
  m_falloff_low_rates.install(nfall, r.low_rate);
  m_rfn_low.push_back(0.0);

  // add this reaction number to the list of falloff reactions
  m_fallindx.push_back(nReactions() - 1);
  m_rfallindx[nReactions() - 1] = nfall;

  // install the enhanced third-body concentration calculator
  map<size_t, double> efficiencies;
  for (const auto &eff : r.third_body.efficiencies) {
    size_t k = kineticsSpeciesIndex(eff.first);
    if (k != npos) {
      efficiencies[k] = eff.second;
    } else if (!m_skipUndeclaredThirdBodies) {
      throw CanteraError("GasKinetics::addFalloffReaction",
                         "Found "
                         "third-body efficiency for undefined species '" +
                             eff.first + "' while adding reaction '" +
                             r.equation() + "'");
    }
  }
  m_falloff_concm.install(nfall, efficiencies, r.third_body.default_efficiency);
  concm_falloff_values.resize(m_falloff_concm.workSize());

  // install the falloff function calculator for this reaction
  m_falloffn.install(nfall, r.reaction_type, r.falloff);
  falloff_work.resize(m_falloffn.workSize());
}

void GasKinetics::addThreeBodyReaction(ThreeBodyReaction &r) {
  m_rates.install(nReactions() - 1, r.rate);
  map<size_t, double> efficiencies;
  for (const auto &eff : r.third_body.efficiencies) {
    size_t k = kineticsSpeciesIndex(eff.first);
    if (k != npos) {
      efficiencies[k] = eff.second;
    } else if (!m_skipUndeclaredThirdBodies) {
      throw CanteraError("GasKinetics::addThreeBodyReaction",
                         "Found "
                         "third-body efficiency for undefined species '" +
                             eff.first + "' while adding reaction '" +
                             r.equation() + "'");
    }
  }
  m_3b_concm.install(nReactions() - 1, efficiencies,
                     r.third_body.default_efficiency);
  concm_3b_values.resize(m_3b_concm.workSize());
}

void GasKinetics::addPlogReaction(PlogReaction &r) {
  m_plog_rates.install(nReactions() - 1, r.rate);
}

void GasKinetics::addChebyshevReaction(ChebyshevReaction &r) {
  m_cheb_rates.install(nReactions() - 1, r.rate);
}

void GasKinetics::modifyReaction(size_t i, shared_ptr<Reaction> rNew) {
  // operations common to all reaction types
  BulkKinetics::modifyReaction(i, rNew);

  switch (rNew->reaction_type) {
  case ELEMENTARY_RXN:
    modifyElementaryReaction(i, dynamic_cast<ElementaryReaction &>(*rNew));
    break;
  case THREE_BODY_RXN:
    modifyThreeBodyReaction(i, dynamic_cast<ThreeBodyReaction &>(*rNew));
    break;
  case FALLOFF_RXN:
  case CHEMACT_RXN:
    modifyFalloffReaction(i, dynamic_cast<FalloffReaction &>(*rNew));
    break;
  case PLOG_RXN:
    modifyPlogReaction(i, dynamic_cast<PlogReaction &>(*rNew));
    break;
  case CHEBYSHEV_RXN:
    modifyChebyshevReaction(i, dynamic_cast<ChebyshevReaction &>(*rNew));
    break;
  default:
    throw CanteraError("GasKinetics::modifyReaction",
                       "Unknown reaction type specified: {}",
                       rNew->reaction_type);
  }

  // invalidate all cached data
  m_ROP_ok = false;
  m_temp += 0.1234;
  m_pres += 0.1234;
}

void GasKinetics::modifyThreeBodyReaction(size_t i, ThreeBodyReaction &r) {
  m_rates.replace(i, r.rate);
}

void GasKinetics::modifyFalloffReaction(size_t i, FalloffReaction &r) {
  size_t iFall = m_rfallindx[i];
  m_falloff_high_rates.replace(iFall, r.high_rate);
  m_falloff_low_rates.replace(iFall, r.low_rate);
  m_falloffn.replace(iFall, r.falloff);
}

void GasKinetics::modifyPlogReaction(size_t i, PlogReaction &r) {
  m_plog_rates.replace(i, r.rate);
}

void GasKinetics::modifyChebyshevReaction(size_t i, ChebyshevReaction &r) {
  m_cheb_rates.replace(i, r.rate);
}

void GasKinetics::init() {
  BulkKinetics::init();
  m_logp_ref = log(thermo().refPressure()) - log(GasConstant);
}

void GasKinetics::invalidateCache() {
  BulkKinetics::invalidateCache();
  m_pres += 0.13579;
}
}
