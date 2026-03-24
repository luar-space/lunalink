lunalink
========

.. raw:: html

   <p>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/test-cpp.yml"><img src="https://github.com/fiskio/lunalink/actions/workflows/test-cpp.yml/badge.svg?branch=main" alt="C++ Tests"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/test-python.yml"><img src="https://github.com/fiskio/lunalink/actions/workflows/test-python.yml/badge.svg?branch=main" alt="Python Tests"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/test-python.yml"><img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-python-coverage.json" alt="Python Coverage"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/coverage-cpp.yml"><img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-region-coverage.json" alt="Region Coverage"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/coverage-cpp.yml"><img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-line-coverage.json" alt="Line Coverage"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/coverage-cpp.yml"><img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-branch-coverage.json" alt="Branch Coverage"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/coverage-cpp.yml"><img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-mcdc-coverage.json" alt="MC/DC Coverage"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/sanitize.yml"><img src="https://github.com/fiskio/lunalink/actions/workflows/sanitize.yml/badge.svg?branch=main" alt="Sanitizers"></a>
     <a href="https://github.com/fiskio/lunalink/actions/workflows/tidy.yml"><img src="https://github.com/fiskio/lunalink/actions/workflows/tidy.yml/badge.svg?branch=main" alt="clang-tidy"></a>
     <a href="https://github.com/fiskio/lunalink/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-Apache%202.0-green.svg" alt="License: Apache 2.0"></a>
   </p>

LunaLink AFS reference implementation with C++ core and pybind11 bindings.

.. toctree::
   :maxdepth: 2
   :caption: Deliverable Verification:

   gateway0
   gateway1
   gateway2
   gateway3
   gateway4
   gateway5
   gateway6
   gateway7
   interop_results

.. toctree::
   :maxdepth: 2
   :caption: Executive Summary:

   evaluation_guide
   roadmap
   references

.. toctree::
   :maxdepth: 2
   :caption: Mission Assurance:

   assurance/public/mission_assurance
   assurance/public/c_plus_plus_guidelines
   assurance/traceability_matrix
   assurance/build_policy
   assurance/configuration_baseline
   assurance/tool_confidence
   assurance/private/fmea
   assurance/private/fault_coverage
   assurance/private/safety_plan
   assurance/private/oss_assessment
   assurance/private/certification_delta
   assurance/private/ecss_tailoring
   assurance/private/recertification_record
   assurance/private/misra_cpp2023_gcs
   assurance/private/static_analysis_report
   assurance/private/cppcheck_report
   assurance/private/srs
   assurance/private/metrics_plan
   assurance/private/praca
   assurance/private/svd
   assurance/private/wcet_profile
   assurance/private/test_procedures
   assurance/private/traceability_matrix
   assurance/private/coverage_exclusions
   signal/compliance_matrix
   signal/shall_inventory
   signal/spec_findings_report
   signal/validation_plan

.. toctree::
   :maxdepth: 2
   :caption: Governance:

   governance/sdp
   governance/scmp
   governance/baseline_register

.. toctree::
   :maxdepth: 2
   :caption: Standards & Compliance:

   standards/esa/ecss_standards
   standards/nasa/npr7150_standards
   standards/nasa_compliance_addendum

.. toctree::
   :maxdepth: 2
   :caption: Technical Reference:

   signal/architecture
   signal/signal_chain
   signal/algorithms
   signal/bch_decoder_design
   signal/bch_performance
   signal/ldpc_performance
   signal/sisicd
   signal/icd

.. toctree::
   :maxdepth: 2
   :caption: Developer & Interoperability:

   quickstart
   developing
   tech_stack
   test_vectors
   versioning
   api
