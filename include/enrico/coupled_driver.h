//! \file coupled_driver.h
//! Base class for driver that controls a coupled physics solve involving neutronics
//! and thermal-hydraulics physics.
#ifndef ENRICO_COUPLED_DRIVER_H
#define ENRICO_COUPLED_DRIVER_H

#include "enrico/driver.h"
#include "enrico/heat_fluids_driver.h"
#include "enrico/neutronics_driver.h"

#include <pugixml.hpp>
#include <xtensor/xtensor.hpp>

#include <map>
#include <memory> // for unique_ptr
#include <vector>

namespace enrico {

//! Base class for driver that controls a coupled physics solve involving neutronics
//! and thermal-hydraulics physics.
class CoupledDriver {
public:
  // Types, aliases
  enum class Norm { L1, L2, LINF }; //! Types of norms

  //! Enumeration of available temperature initial condition specifications.
  //! 'neutronics' sets temperature condition from the neutronics input files,
  //! while 'heat' sets temperature based on a thermal-fluids input (or restart) file.
  enum class Initial { neutronics, heat };

  //! Initializes coupled neutron transport and thermal-hydraulics solver with
  //! the given MPI communicator
  //!
  //! \param comm The MPI communicator used for the coupled driver
  //! \param node XML node containing settings
  CoupledDriver(MPI_Comm comm, pugi::xml_node node);

  ~CoupledDriver() {}

  //! Execute the coupled driver
  virtual void execute();

  //! Update the heat source for the thermal-hydraulics solver
  //!
  //! \param relax Apply relaxation to heat source before updating heat solver
  void update_heat_source(bool relax);

  //! Update the temperature for the neutronics solver
  //!
  //! \param relax Apply relaxation to temperature before updating neutronics solver
  void update_temperature(bool relax);

  //! Update the density for the neutronics solver
  //!
  //! \param relax Apply relaxation to density before updating neutronics solver
  void update_density(bool relax);

  //! Check convergence of the coupled solve for the current Picard iteration.
  bool is_converged();

  //! Compute the norm of the temperature between two successive Picard iterations
  //! \param norm enumeration of norm to compute
  //! \return norm of the temperature between two iterations
  double temperature_norm(Norm n);

  //! Get reference to neutronics driver
  //! \return reference to driver
  NeutronicsDriver& get_neutronics_driver() const { return *neutronics_driver_; }

  //! Get reference to thermal-fluids driver
  //! \return reference to driver
  HeatFluidsDriver& get_heat_driver() const { return *heat_fluids_driver_; }

  //! Get timestep iteration index
  //! \return timestep iteration index
  int get_timestep_index() const { return i_timestep_; }

  //! Get Picard iteration index within current timestep
  //! \return Picard iteration index within current timestep
  int get_picard_index() const { return i_picard_; }

  //! Whether solve is for first Picard iteration of first timestep
  bool is_first_iteration() const
  {
    return get_timestep_index() == 0 and get_picard_index() == 0;
  }

  Comm comm_; //!< The MPI communicator used to run the driver

  double power_; //!< Power in [W]

  int max_timesteps_; //!< Maximum number of time steps

  int max_picard_iter_; //!< Maximum number of Picard iterations

  //! Picard iteration convergence tolerance, defaults to 1e-3 if not set
  double epsilon_{1e-3};

  //! Constant relaxation factor for the heat source,
  //! defaults to 1.0 (standard Picard) if not set
  double alpha_{1.0};

  //! Constant relaxation factor for the temperature, defaults to the
  //! relaxation aplied to the heat source if not set
  double alpha_T_{alpha_};

  //! Constant relaxation factor for the density, defaults to the
  //! relaxation applied to the heat source if not set
  double alpha_rho_{alpha_};

  //! Where to obtain the temperature initial condition from. Defaults to the
  //! temperatures in the neutronics input file.
  Initial temperature_ic_{Initial::neutronics};

  //! Where to obtain the density initial condition from. Defaults to the densities
  //! in the neutronics input file.
  Initial density_ic_{Initial::neutronics};

private:
  //! Create bidirectional mappings from neutronics cell instances to/from TH elements
  void init_mappings();

  //! Initialize the Monte Carlo tallies for all cells
  void init_tallies();

  //! Initialize global volume buffers for neutronics ranks
  void init_volumes();

  //! Initialize global fluid masks on all TH ranks.
  void init_elem_fluid_mask();

  //! Initialize fluid masks for neutronics cells on all neutronic ranks.
  void init_cell_fluid_mask();

  //! Initialize current and previous Picard temperature fields
  void init_temperatures();

  //! Initialize current and previous Picard density fields
  void init_densities();

  //! Initialize current and previous Picard heat source fields. Note that
  //! because the neutronics solver is assumed to run first, that no initial
  //! condition is required for the heat source. So, unlike init_temperatures(),
  //! this method does not set any initial values.
  void init_heat_source();

  //! Print report of communicator layout
  void comm_report();

  //! Special alpha value indicating use of Robbins-Monro relaxation
  constexpr static double ROBBINS_MONRO = -1.0;

  int i_timestep_; //!< Index pertaining to current timestep

  int i_picard_; //!< Index pertaining to current Picard iteration

  //! The rank in comm_ that corresponds to the root of the neutronics comm
  int neutronics_root_ = MPI_PROC_NULL;

  //! The rank in comm_ that corresponds to the root of the heat comm
  int heat_root_ = MPI_PROC_NULL;

  //! List of ranks in this->comm_ that are in the heat/fluids subcomm
  std::vector<int> heat_ranks_;

  //! List of ranks in this->comm_ that are in the neutronics subcomm
  std::vector<int> neutronics_ranks_;

  //! Current Picard iteration temperature for the local cells.
  //! This temperature is computed by the heat/fluids solver and averaged over the
  //! "local cells", which are the portions of the neutronics cells that are in
  //! a given heat-fluid subdomain.
  xt::xtensor<double, 1> l_cell_temps_;

  //! Previous Picard iteration temperature for the local cells.
  xt::xtensor<double, 1> l_cell_temps_prev_;

  //! Current Picard iteration density; this density is the density
  //! computed by the thermal-hydraulic solver, and data mappings may result in
  //! a different density actually used in the neutronics solver. For example,
  //! the entries in this xtensor may be averaged over neutronics cells to give
  //! the density used by the neutronics solver.
  xt::xtensor<double, 1> densities_;

  xt::xtensor<double, 1> densities_prev_; //!< Previous Picard iteration density

  //! Current Picard iteration heat source; this heat source is the heat source
  //! computed by the neutronics solver, and data mappings may result in a different
  //! heat source actually used in the heat solver. For example, the entries in this
  //! xtensor may be averaged over thermal-hydraulics cells to give the heat source
  //! used by the thermal-hydraulics solver.
  xt::xtensor<double, 1> heat_source_;

  xt::xtensor<double, 1> heat_source_prev_; //!< Previous Picard iteration heat source

  std::unique_ptr<NeutronicsDriver> neutronics_driver_;  //!< The neutronics driver
  std::unique_ptr<HeatFluidsDriver> heat_fluids_driver_; //!< The heat-fluids driver

  //! States whether a global element is in the fluid region
  //! These are **not** ordered by TH global element indices.  Rather, these are
  //! ordered according to an MPI_Gatherv operation on TH local elements.
  std::vector<int> elem_fluid_mask_;

  //! States whether a neutronic cell is in the fluid region
  xt::xtensor<int, 1> cell_fluid_mask_;

  //! Volumes of global elements in TH solver
  //! These are **not** ordered by TH global element indices.  Rather, these are
  //! ordered according to an MPI_Gatherv operation on TH local elements.
  // std::vector<double> elem_volumes_;

  //! Map that gives a list of TH element indices for a given neutronics cell
  //! handle. The TH element indices refer to indices defined by the MPI_Gatherv
  //! operation, and do not reflect TH internal global element indexing.
  // std::unordered_map<CellHandle, std::vector<int32_t>> cell_to_elems_;

  //! Map that gives the neutronics cell handle for a given TH element index.
  //! The TH element indices refer to indices defined by the MPI_Gatherv
  //! operation, and do not reflect TH internal global element indexing.
  // std::vector<CellHandle> elem_to_cell_;

  //! Map TH local element id -> neutronics cell.
  //! Unlike elem_to_cell_, the elem IDs are local IDs internal to the TH driver
  //! element IDs.
  //! Persists only on ranks where the heat driver is active.
  std::vector<CellHandle> l_elem_to_g_cell_;

  //! Maps global cell ID to local elem IDs
  //! Ordering of keys (global cell IDs) is the same as ordering of l_cell_to_g_cell
  //! and l_cell_volume.  This is because l_cell_to_g_cell and l_cell_volume are both
  //! contructed by iterating through the keys of this map in order.
  std::map<CellHandle, std::vector<int32_t>> g_cell_to_l_elems_;

  std::map<CellHandle, CellHandle> g_cell_to_l_cell_;

  // Maps local cell ID (vector index) to global cell ID (vector value)
  std::vector<CellHandle> l_cell_to_g_cell_;

  //! Maps local cell ID (vector index) to local cell volume (vector value)
  //! TODO: xtensor
  std::vector<double> l_cell_volumes_;

  //! Maps local element ID (vector index) to local elem volume (vector value)
  //! TODO: xtensor
  std::vector<double> l_elem_volumes_;

  //! Number of unique neutronics cells in heat subdomain
  CellHandle n_local_cells_;

  //! Number of unique cells in neutronics model
  CellHandle n_global_cells_;

  //! Number of global elements in heat/fluids model
  int32_t n_global_elem_;

  // Norm to use for convergence checks
  Norm norm_{Norm::LINF};
};

} // namespace enrico

#endif // ENRICO_COUPLED_DRIVER_H
