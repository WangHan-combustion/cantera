/**
 *  @file RxnActiveMgr.cpp Declarations for the base class for active reaction
 *  managers
 *
 *  RxnActiveMgr managers the activation of reactions in a adpative kinitics
 *  scheme.
 */
// Copyright 2016  Hao Wu (wuhao@stanford.edu)

#include "cantera/kinetics/Kinetics.h"
#include "cantera/kinetics/RxnActiveMgr.h"

using namespace std;

namespace Cantera
{

void RxnActiveMgr::updateStoichMatrix()
{
  typedef Eigen::Triplet<double> T;

  vector<T> tripletList;
  double coeff;

  AssertThrowMsg(m_kinetics->nPhases() == 1, "RxnActiveMgr::updateStoichMatrix",
                 "Only homogeneous reaction mechanisms are supported, nPhases = {} != 1", m_kinetics->nPhases());

  // Update nRxns and nSpecs
  size_t _nSpecs = m_kinetics->thermo().nSpecies();
  size_t _nRxns = m_kinetics->nReactions();

  // Resize stoich. matrix
  resizeData(_nSpecs, _nRxns);

  // Fill in molar stoich matrix first
  tripletList.clear();
  for (size_t iRxn = 0; iRxn < m_nRxns; iRxn++) {
    for (size_t iSpec = 0; iSpec < m_nSpecs; iSpec++) {
       coeff = m_kinetics->productStoichCoeff(iSpec, iRxn);
      if (coeff != 0.)
        tripletList.push_back(T(iSpec,iRxn,coeff));
      coeff = m_kinetics->reactantStoichCoeff(iSpec, iRxn);
      if (coeff != 0.)
        tripletList.push_back(T(iSpec,iRxn,-coeff));
    }
  }
  m_stoich_mol.setFromTriplets(tripletList.begin(), tripletList.end());
  m_stoich_mol.makeCompressed();

  // Update non-zero patterns for m_wm
  m_wm = m_stoich_mol;
  m_wm.setZero();
}

void RxnActiveMgr::updateActiveRxns(const double relTol, const double absTol)
{
  // Reset m_iactive
  std::fill(m_iactive.begin(), m_iactive.end(), true);
  // Obtain temperature
  const double T = m_kinetics->thermo().temperature();
  // Obtain density
  const double rho = m_kinetics->thermo().density();
  // Obtain cv
  const double cv = m_kinetics->thermo().cv_mass();

  // Stoich * ROP
  VecType& ROP = m_wa_nr;
  m_kinetics->getNetRatesOfProgress(ROP.data());
  m_wm = m_stoich_mol * ROP.asDiagonal();

  // dTV = 1/(rTol * T + aTol) * 1/rho * 1/cp * (-u') * (Stoich * ROP)
  VecType& u = m_wa_ns;
  m_kinetics->thermo().getPartialMolarIntEnergies(u.data());
  u /= (-rho * cv * (relTol*T + absTol));
  VecType& dTVec = m_wa_nr;
  dTVec = m_wm.transpose() * u;

  // dYV = diag(1 / (rTol * Y + aTol)) * 1/rho * daig(MW) * (Stoich * ROP)
  SpCMat& dYMat = m_wm;
  VecType& Y = m_wa_ns;
  m_kinetics->thermo().getMassFractions(Y.data());
  Y *= rho*relTol;
  Y.array() += (rho*absTol);
  Y.noalias() = Y.cwiseInverse();
  const vector<double>& mw = m_kinetics->thermo().molecularWeights();
  Y *= Eigen::Map<const VecType>(mw.data(), m_nSpecs);
  dYMat = Y.asDiagonal() * m_wm;
  
  // choose activated reactions
  double dTError;
  VecType& dYError = m_wa_ns;
  double tmpError;
  dTError = 0.;
  dYError.setZero();
  for (size_t iRxn = 0; iRxn < m_nRxns; iRxn++) {
    tmpError = dTError + dTVec[iRxn];
    if (abs(tmpError) > 1.) continue; // error in T is too large
    bool _iact = false;
    for (Eigen::SparseMatrix<double>::InnerIterator it(dYMat,iRxn); it; ++it) {
      // it.row() == iSpec
      tmpError = dYError[it.row()] + it.value();
      if (abs(tmpError) > 1.) { // error in Y[it.row()] is too large
        _iact = true;
        break;
      }
    }
    if (!_iact) { // iRxn shall no be activated
      // change activation flag
      m_iactive[iRxn] = false;
      // update accululated error
      dTError += dTVec[iRxn];
      dYError += dYMat.col(iRxn);
    }
  }
}

void RxnActiveMgr::resizeData(const size_t _nSpecs, const size_t _nRxns)
{
  if (_nSpecs==m_nSpecs && _nRxns==m_nRxns)
    return;
  m_nSpecs = _nSpecs; m_nRxns = _nRxns;
  // Resize stoich matrix
  m_stoich_mol.resize(m_nSpecs, m_nRxns);
  // Resize working arrays
  m_wa_ns.resize(m_nSpecs);
  m_wa_nr.resize(m_nRxns);
  m_wm.resize(m_nSpecs, m_nRxns);
  // Resize activation flag
  m_iactive.resize(m_nRxns);
}

}