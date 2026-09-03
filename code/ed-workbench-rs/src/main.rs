use std::fs;
use std::path::PathBuf;
use std::process::ExitCode;
use std::time::Instant;

use clap::{Parser, Subcommand, ValueEnum};
use ed_workbench_rs::ao2mo::transform_to_mo;
use ed_workbench_rs::benchmark::{
    BoundedBenchmarkConfig, BoundedBenchmarkResult, run_h2o_cc_pvdz_benchmark,
};
use ed_workbench_rs::coupled_cluster::{CcConfig, CcTermination, solve_cc, solve_cc_series};
use ed_workbench_rs::davidson::{
    DavidsonConfig, DavidsonRunConfig, DavidsonWorkspaceConfig, lowest_eigenpair,
    lowest_eigenpair_with_run_config, lowest_eigenpairs,
};
use ed_workbench_rs::dense_fci::ground_state_energy;
use ed_workbench_rs::determinant::DeterminantBasis;
use ed_workbench_rs::direct_fci::{DirectFciOperator, ExecutionPolicy};
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::libcint_frontend::{ENERGY_UNIT, compute_ao_integrals};
use ed_workbench_rs::mbpt::solve_mbpt;
use ed_workbench_rs::molecule::Molecule;
use ed_workbench_rs::operator::LinearOperator;
use ed_workbench_rs::optimizer::BfgsConfig;
use ed_workbench_rs::problem::ElectronicProblem;
use ed_workbench_rs::published_reference::{HirataTable2, SeriesKind, rounded_published_match};
use ed_workbench_rs::reference::{Reference, sha256_hex};
use ed_workbench_rs::rhf::{RhfConfig, solve_rhf};
use ed_workbench_rs::truncated_ci::{solve_ci, solve_ci_series};
use ed_workbench_rs::unitary_cc::UnitaryCcModel;
use serde::Serialize;

#[derive(Debug, Parser)]
#[command(name = "ed-workbench-rs")]
#[command(about = "Rust electronic-structure workbench for Quantum Harness #129")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    Inspect {
        fcidump: PathBuf,
    },
    DenseFci {
        fcidump: PathBuf,
    },
    Davidson {
        fcidump: PathBuf,
        #[arg(long, default_value_t = 1e-8)]
        residual_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
        #[arg(long, default_value_t = 24)]
        max_subspace: usize,
        #[arg(long)]
        workspace: Option<PathBuf>,
        #[arg(long, requires = "workspace")]
        resume: bool,
        #[arg(long, default_value_t = 1, requires = "workspace")]
        checkpoint_every: usize,
        #[arg(
            long,
            default_value_t = 2.0,
            help = "conservative Davidson-vector budget in GiB; not an operating-system hard limit"
        )]
        memory_budget_gib: f64,
        #[arg(long, requires = "workspace")]
        operator_fingerprint: Option<String>,
        #[arg(
            long,
            default_value_t = 1,
            help = "fixed ordered source blocks; values greater than one enable parallel sigma"
        )]
        parallel_blocks: usize,
        #[arg(long, default_value_t = 2.0)]
        parallel_memory_budget_gib: f64,
        #[arg(long)]
        strict_parallel_memory: bool,
    },
    SigmaBenchmark {
        fcidump: PathBuf,
    },
    DavidsonRoots {
        fcidump: PathBuf,
        #[arg(long)]
        roots: usize,
        #[arg(long, default_value_t = 1e-8)]
        residual_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
        #[arg(long, default_value_t = 24)]
        max_subspace: usize,
    },
    Cc {
        fcidump: PathBuf,
        reference: PathBuf,
        #[arg(long)]
        rank: usize,
        #[arg(long, default_value_t = 1e-8)]
        residual_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
    },
    CcSeries {
        fcidump: PathBuf,
        reference: PathBuf,
        #[arg(long)]
        published_reference: Option<PathBuf>,
        #[arg(long, default_value_t = 8)]
        max_rank: usize,
        #[arg(long, default_value_t = 1e-6)]
        residual_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
        #[arg(long)]
        json_output: Option<PathBuf>,
    },
    Ci {
        fcidump: PathBuf,
        #[arg(long)]
        rank: usize,
        #[arg(long, default_value_t = 1e-9)]
        residual_tolerance: f64,
    },
    Mbpt {
        fcidump: PathBuf,
        reference: PathBuf,
        #[arg(long)]
        order: usize,
    },
    Level3Series {
        fcidump: PathBuf,
        reference: PathBuf,
        #[arg(long)]
        published_reference: Option<PathBuf>,
        #[arg(long, default_value_t = 8)]
        max_ci_rank: usize,
        #[arg(long, default_value_t = 20)]
        max_mbpt_order: usize,
        #[arg(long, default_value_t = 1e-7)]
        ci_residual_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
        #[arg(long, default_value_t = 24)]
        max_subspace: usize,
    },
    Ucc {
        fcidump: PathBuf,
        #[arg(long)]
        rank: usize,
        #[arg(long, default_value_t = 1e-7)]
        gradient_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
    },
    Rhf {
        #[arg(value_enum)]
        system: DirectSystem,
    },
    DirectIntegralsFci {
        #[arg(value_enum)]
        system: DirectSystem,
        #[arg(long, default_value_t = 1e-9)]
        residual_tolerance: f64,
        #[arg(long, default_value_t = 100)]
        max_iterations: usize,
        #[arg(long, default_value_t = 24)]
        max_subspace: usize,
        #[arg(
            long,
            default_value_t = 1,
            help = "fixed ordered source blocks; values greater than one enable parallel sigma"
        )]
        parallel_blocks: usize,
        #[arg(long, default_value_t = 2.0)]
        parallel_memory_budget_gib: f64,
        #[arg(long)]
        strict_parallel_memory: bool,
    },
    Benchmark {
        #[arg(value_enum)]
        system: BenchmarkSystem,
        #[arg(long, default_value_t = 16)]
        sources: usize,
        #[arg(
            long = "memory-budget-gib",
            visible_alias = "max-memory-gib",
            default_value_t = 2.0,
            help = "not an operating-system hard memory limit; rejects conservative estimates above this GiB budget"
        )]
        memory_budget_gib: f64,
        #[arg(long)]
        json_output: Option<PathBuf>,
    },
    Verify {
        fcidump: PathBuf,
        reference: PathBuf,
        #[arg(long, default_value_t = 1e-10)]
        tolerance: f64,
    },
}

#[derive(Debug, Clone, Copy, ValueEnum)]
enum DirectSystem {
    H2Sto3g,
    H2oSto3g,
}

#[derive(Debug, Clone, Copy, ValueEnum)]
enum BenchmarkSystem {
    H2oCcPvdz,
}

#[derive(Debug, Serialize)]
struct CcSeriesJson {
    schema_version: u32,
    artifact_kind: &'static str,
    system: String,
    energy_unit: &'static str,
    fci_reference_energy: f64,
    config: CcConfig,
    results: Vec<CcRankJson>,
    published_verification: Option<bool>,
}

#[derive(Debug, Serialize)]
struct CcRankJson {
    rank: usize,
    energy: f64,
    method_minus_fci: f64,
    iterations: usize,
    residual_norm: f64,
    elapsed_seconds: f64,
    converged: bool,
    termination: CcTermination,
    published_difference: Option<f64>,
    published_error: Option<f64>,
    published_match: Option<bool>,
}

impl DirectSystem {
    fn molecule(self) -> Molecule {
        match self {
            Self::H2Sto3g => Molecule::h2_sto3g(),
            Self::H2oSto3g => Molecule::h2o_sto3g(),
        }
    }

    fn label(self) -> &'static str {
        match self {
            Self::H2Sto3g => "H2/STO-3G",
            Self::H2oSto3g => "H2O/STO-3G",
        }
    }
}

fn main() -> ExitCode {
    match run(Cli::parse()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("error: {error}");
            ExitCode::FAILURE
        }
    }
}

fn run(cli: Cli) -> Result<(), Box<dyn std::error::Error>> {
    match cli.command {
        Command::Inspect { fcidump } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let basis = DeterminantBasis::with_symmetry(
                dump.norb,
                dump.nelec,
                dump.ms2,
                &dump.orbsym,
                dump.isym,
            )?;
            println!("NORB: {}", dump.norb);
            println!("NELEC: {}", dump.nelec);
            println!("MS2: {}", dump.ms2);
            println!("ORBSYM: {:?}", dump.orbsym);
            println!("ISYM: {}", dump.isym);
            println!("ECORE: {:.16}", dump.ecore);
            println!("one-electron records: {}", dump.one_body_record_count());
            println!("two-electron records: {}", dump.two_body_record_count());
            println!("alpha strings: {}", basis.alpha_strings.len());
            println!("beta strings: {}", basis.beta_strings.len());
            println!("determinants: {} (ISYM={} sector)", basis.len(), dump.isym);
        }
        Command::DenseFci { fcidump } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            println!("{:.15}", ground_state_energy(&dump)?);
        }
        Command::Davidson {
            fcidump,
            residual_tolerance,
            max_iterations,
            max_subspace,
            workspace,
            resume,
            checkpoint_every,
            memory_budget_gib,
            operator_fingerprint,
            parallel_blocks,
            parallel_memory_budget_gib,
            strict_parallel_memory,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let problem = ElectronicProblem::from_fcidump(&dump)?;
            let operator =
                DirectFciOperator::new(problem)?.with_execution_policy(execution_policy(
                    parallel_blocks,
                    parallel_memory_budget_gib,
                    strict_parallel_memory,
                )?)?;
            let mut initial = vec![0.0; operator.dimension()];
            let reference_index = operator
                .diagonal()
                .iter()
                .enumerate()
                .min_by(|(_, left), (_, right)| left.total_cmp(right))
                .map(|(index, _)| index)
                .ok_or("empty determinant basis")?;
            initial[reference_index] = 1.0;
            let algorithm = DavidsonConfig {
                residual_tolerance,
                energy_tolerance: residual_tolerance * 0.01,
                max_iterations,
                max_subspace,
            };
            let estimated_bytes =
                davidson_resident_bytes(operator.dimension(), max_subspace, workspace.is_some())?;
            let budget_bytes = gib_to_bytes(memory_budget_gib)?;
            if estimated_bytes > budget_bytes {
                return Err(format!(
                    "conservative Davidson vector estimate {:.6} GiB exceeds budget {:.6} GiB",
                    estimated_bytes as f64 / GIB,
                    budget_bytes as f64 / GIB
                )
                .into());
            }
            let run_config = DavidsonRunConfig {
                algorithm,
                workspace: workspace.map(|path| DavidsonWorkspaceConfig {
                    path,
                    resume,
                    checkpoint_every,
                    operator_fingerprint: operator_fingerprint
                        .unwrap_or_else(|| sha256_hex(&bytes)),
                }),
            };
            let result = lowest_eigenpair_with_run_config(&operator, &initial, &run_config)?;
            println!("energy: {:.15}", result.energy);
            println!("residual norm: {:.3e}", result.residual_norm);
            println!("iterations: {}", result.iterations);
            println!(
                "storage: {}",
                if run_config.workspace.is_some() {
                    "disk workspace"
                } else {
                    "memory"
                }
            );
            println!(
                "conservative Davidson vectors: {:.6} GiB",
                estimated_bytes as f64 / GIB
            );
            print_execution_preflight(&operator);
            println!("converged: {}", result.converged);
            if !result.converged {
                return Err("Davidson did not converge".into());
            }
        }
        Command::SigmaBenchmark { fcidump } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let build_started = Instant::now();
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let build_elapsed = build_started.elapsed();
            let input: Vec<_> = (0..operator.dimension())
                .map(|index| ((index * 17 + 3) as f64).sin())
                .collect();
            let mut output = vec![0.0; operator.dimension()];
            let apply_started = Instant::now();
            operator.apply(&input, &mut output)?;
            let apply_elapsed = apply_started.elapsed();
            let output_norm = output.iter().map(|value| value * value).sum::<f64>().sqrt();
            let checksum = output
                .iter()
                .enumerate()
                .map(|(index, value)| (index % 97 + 1) as f64 * value)
                .sum::<f64>();
            println!("determinants: {}", operator.dimension());
            println!("rayon threads: {}", rayon::current_num_threads());
            println!("operator build seconds: {:.6}", build_elapsed.as_secs_f64());
            println!("sigma apply seconds: {:.6}", apply_elapsed.as_secs_f64());
            println!("output norm: {:.15e}", output_norm);
            println!("weighted checksum: {:.15e}", checksum);
        }
        Command::DavidsonRoots {
            fcidump,
            roots,
            residual_tolerance,
            max_iterations,
            max_subspace,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let results = lowest_eigenpairs(
                &operator,
                roots,
                &DavidsonConfig {
                    residual_tolerance,
                    energy_tolerance: residual_tolerance * 0.01,
                    max_iterations,
                    max_subspace,
                },
            )?;
            println!(
                "root\tenergy_hartree\texcitation_energy_hartree\titerations\tresidual\tconverged"
            );
            let ground = results[0].energy;
            for (root, result) in results.iter().enumerate() {
                println!(
                    "{}\t{:.15}\t{:.15}\t{}\t{:.3e}\t{}",
                    root,
                    result.energy,
                    result.energy - ground,
                    result.iterations,
                    result.residual_norm,
                    result.converged
                );
            }
            if results.iter().any(|result| !result.converged) {
                return Err("multi-root Davidson did not converge".into());
            }
        }
        Command::Cc {
            fcidump,
            reference,
            rank,
            residual_tolerance,
            max_iterations,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let reference = Reference::load(&reference)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let result = solve_cc(
                &operator,
                rank,
                &reference.active_orbital_energies,
                &CcConfig {
                    residual_tolerance,
                    energy_tolerance: residual_tolerance * 0.01,
                    max_iterations,
                    ..Default::default()
                },
            )?;
            for item in &result.iterations {
                println!(
                    "iter {:3}  E={:.15}  dE={:.3e}  |R|={:.3e}",
                    item.iteration, item.energy, item.energy_change, item.residual_norm
                );
            }
            println!("CC({rank}) energy: {:.15}", result.energy);
            println!("residual norm: {:.3e}", result.residual_norm);
            println!("converged: {}", result.converged);
            if !result.converged {
                return Err(format!("CC({rank}) did not converge").into());
            }
        }
        Command::CcSeries {
            fcidump,
            reference,
            published_reference,
            max_rank,
            residual_tolerance,
            max_iterations,
            json_output,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let reference = Reference::load(&reference)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let published = published_reference
                .as_deref()
                .map(HirataTable2::load)
                .transpose()?;
            if let Some(table) = &published {
                validate_hirata_context(table, &reference, &operator)?;
            }
            let cc_config = CcConfig {
                residual_tolerance,
                energy_tolerance: residual_tolerance * 0.01,
                max_iterations,
                ..Default::default()
            };
            let series = solve_cc_series(
                &operator,
                max_rank,
                &reference.active_orbital_energies,
                &cc_config,
            )?;

            println!("system: {}", reference.system);
            println!("energy unit: hartree");
            println!(
                "rank\tenergy_hartree\tmethod_minus_fci_hartree\titerations\tresidual\telapsed_seconds\tconverged\tpublished_difference\tpublished_error\tpublished_match"
            );
            let mut published_matches = true;
            let mut json_results = Vec::with_capacity(series.len());
            for entry in &series {
                let difference = entry.result.energy - reference.fci_energy;
                let iterations = entry.result.iterations.len();
                if let Some(table) = &published {
                    let expected = table
                        .difference(SeriesKind::Cc, entry.rank)
                        .ok_or_else(|| format!("Hirata Table 2 has no CC({}) value", entry.rank))?;
                    let error = difference - expected;
                    let matches =
                        rounded_published_match(difference, expected, table.printed_decimals);
                    published_matches &= matches;
                    json_results.push(CcRankJson {
                        rank: entry.rank,
                        energy: entry.result.energy,
                        method_minus_fci: difference,
                        iterations,
                        residual_norm: entry.result.residual_norm,
                        elapsed_seconds: entry.elapsed.as_secs_f64(),
                        converged: entry.result.converged,
                        termination: entry.result.termination,
                        published_difference: Some(expected),
                        published_error: Some(error),
                        published_match: Some(matches),
                    });
                    println!(
                        "{}\t{:.15}\t{:.15}\t{}\t{:.3e}\t{:.6}\t{}\t{:.6}\t{:.3e}\t{}",
                        entry.rank,
                        entry.result.energy,
                        difference,
                        iterations,
                        entry.result.residual_norm,
                        entry.elapsed.as_secs_f64(),
                        entry.result.converged,
                        expected,
                        error,
                        matches
                    );
                } else {
                    json_results.push(CcRankJson {
                        rank: entry.rank,
                        energy: entry.result.energy,
                        method_minus_fci: difference,
                        iterations,
                        residual_norm: entry.result.residual_norm,
                        elapsed_seconds: entry.elapsed.as_secs_f64(),
                        converged: entry.result.converged,
                        termination: entry.result.termination,
                        published_difference: None,
                        published_error: None,
                        published_match: None,
                    });
                    println!(
                        "{}\t{:.15}\t{:.15}\t{}\t{:.3e}\t{:.6}\t{}\t-\t-\t-",
                        entry.rank,
                        entry.result.energy,
                        difference,
                        iterations,
                        entry.result.residual_norm,
                        entry.elapsed.as_secs_f64(),
                        entry.result.converged
                    );
                }
            }
            let converged =
                series.len() == max_rank && series.iter().all(|entry| entry.result.converged);
            println!("series converged: {converged}");
            if published.is_some() {
                println!(
                    "published verification: {}",
                    if published_matches && converged {
                        "PASS"
                    } else {
                        "FAIL"
                    }
                );
            }
            if let Some(path) = json_output {
                if let Some(parent) = path.parent()
                    && !parent.as_os_str().is_empty()
                {
                    fs::create_dir_all(parent)?;
                }
                let summary = CcSeriesJson {
                    schema_version: 1,
                    artifact_kind: "cc-series",
                    system: reference.system.clone(),
                    energy_unit: "hartree",
                    fci_reference_energy: reference.fci_energy,
                    config: cc_config,
                    results: json_results,
                    published_verification: published
                        .as_ref()
                        .map(|_| published_matches && converged),
                };
                fs::write(&path, serde_json::to_vec_pretty(&summary)?)?;
                println!("JSON output: {}", path.display());
            }
            if !converged {
                return Err(format!("CC series stopped before converged CC({max_rank})").into());
            }
            if !published_matches {
                return Err("CC series does not match Hirata 2000 Table 2".into());
            }
        }
        Command::Ci {
            fcidump,
            rank,
            residual_tolerance,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let result = solve_ci(
                &operator,
                rank,
                &DavidsonConfig {
                    residual_tolerance,
                    energy_tolerance: residual_tolerance * 0.01,
                    max_iterations: 100,
                    max_subspace: 24,
                },
            )?;
            println!("CI({rank}) energy: {:.15}", result.energy);
            println!("residual norm: {:.3e}", result.residual_norm);
            println!("iterations: {}", result.iterations);
            println!("converged: {}", result.converged);
            if !result.converged {
                return Err(format!("CI({rank}) did not converge").into());
            }
        }
        Command::Mbpt {
            fcidump,
            reference,
            order,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let reference = Reference::load(&reference)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let result = solve_mbpt(&operator, &reference.active_orbital_energies, order)?;
            println!("reference energy: {:.15}", result.reference_energy);
            for index in 0..order {
                println!(
                    "order {:2}: correction={:.15e}  total={:.15}",
                    index + 1,
                    result.corrections[index],
                    result.partial_sums[index]
                );
            }
        }
        Command::Level3Series {
            fcidump,
            reference,
            published_reference,
            max_ci_rank,
            max_mbpt_order,
            ci_residual_tolerance,
            max_iterations,
            max_subspace,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let reference = Reference::load(&reference)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let published = published_reference
                .as_deref()
                .map(HirataTable2::load)
                .transpose()?;
            if let Some(table) = &published {
                validate_hirata_context(table, &reference, &operator)?;
            }

            let ci_series = solve_ci_series(
                &operator,
                max_ci_rank,
                &DavidsonConfig {
                    residual_tolerance: ci_residual_tolerance,
                    energy_tolerance: ci_residual_tolerance * 0.01,
                    max_iterations,
                    max_subspace,
                },
            )?;
            let mbpt_started = Instant::now();
            let mbpt = solve_mbpt(
                &operator,
                &reference.active_orbital_energies,
                max_mbpt_order,
            )?;
            let mbpt_elapsed = mbpt_started.elapsed();

            println!("system: {}", reference.system);
            println!("energy unit: hartree");
            println!("CI series");
            println!(
                "method\torder\tdimension\tenergy_hartree\tmethod_minus_fci_hartree\titerations\tresidual\telapsed_seconds\tconverged\tpublished_difference\tpublished_error\tpublished_match"
            );
            let mut published_matches = true;
            for entry in &ci_series {
                let difference = entry.result.energy - reference.fci_energy;
                if let Some(table) = &published {
                    let expected = table
                        .difference(SeriesKind::Ci, entry.rank)
                        .ok_or_else(|| format!("Hirata Table 2 has no CI({}) value", entry.rank))?;
                    let error = difference - expected;
                    let matches =
                        rounded_published_match(difference, expected, table.printed_decimals);
                    published_matches &= matches;
                    println!(
                        "CI\t{}\t{}\t{:.15}\t{:.15}\t{}\t{:.3e}\t{:.6}\t{}\t{:.6}\t{:.3e}\t{}",
                        entry.rank,
                        entry.dimension,
                        entry.result.energy,
                        difference,
                        entry.result.iterations,
                        entry.result.residual_norm,
                        entry.elapsed.as_secs_f64(),
                        entry.result.converged,
                        expected,
                        error,
                        matches
                    );
                } else {
                    println!(
                        "CI\t{}\t{}\t{:.15}\t{:.15}\t{}\t{:.3e}\t{:.6}\t{}\t-\t-\t-",
                        entry.rank,
                        entry.dimension,
                        entry.result.energy,
                        difference,
                        entry.result.iterations,
                        entry.result.residual_norm,
                        entry.elapsed.as_secs_f64(),
                        entry.result.converged
                    );
                }
            }
            let ci_converged = ci_series.len() == max_ci_rank
                && ci_series.iter().all(|entry| entry.result.converged);
            println!("CI series converged: {ci_converged}");

            println!("MBPT series");
            println!(
                "method\torder\tenergy_hartree\tmethod_minus_fci_hartree\tcorrection_hartree\tpublished_difference\tpublished_error\tpublished_match"
            );
            for order in 1..=max_mbpt_order {
                let energy = mbpt.partial_sums[order - 1];
                let difference = energy - reference.fci_energy;
                if let Some(table) = &published {
                    let expected = table
                        .difference(SeriesKind::Mbpt, order)
                        .ok_or_else(|| format!("Hirata Table 2 has no MBPT({order}) value"))?;
                    let error = difference - expected;
                    let matches =
                        rounded_published_match(difference, expected, table.printed_decimals);
                    published_matches &= matches;
                    println!(
                        "MBPT\t{}\t{:.15}\t{:.15}\t{:.15e}\t{:.6}\t{:.3e}\t{}",
                        order,
                        energy,
                        difference,
                        mbpt.corrections[order - 1],
                        expected,
                        error,
                        matches
                    );
                } else {
                    println!(
                        "MBPT\t{}\t{:.15}\t{:.15}\t{:.15e}\t-\t-\t-",
                        order,
                        energy,
                        difference,
                        mbpt.corrections[order - 1]
                    );
                }
            }
            println!("MBPT elapsed seconds: {:.6}", mbpt_elapsed.as_secs_f64());
            if published.is_some() {
                println!(
                    "published verification: {}",
                    if published_matches && ci_converged {
                        "PASS"
                    } else {
                        "FAIL"
                    }
                );
            }
            if !ci_converged {
                return Err(format!("CI series stopped before converged CI({max_ci_rank})").into());
            }
            if !published_matches {
                return Err("CI/MBPT series do not match Hirata 2000 Table 2".into());
            }
        }
        Command::Ucc {
            fcidump,
            rank,
            gradient_tolerance,
            max_iterations,
        } => {
            let bytes = fs::read(&fcidump)?;
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump)?)?;
            let model = UnitaryCcModel::new(&operator, rank)?;
            let result = model.optimize(&BfgsConfig {
                gradient_tolerance,
                max_iterations,
                finite_difference_step: 1e-5,
            });
            println!("UCC({rank}) energy: {:.15}", result.value);
            println!("gradient norm: {:.3e}", result.gradient_norm);
            println!("iterations: {}", result.iterations);
            println!("parameters: {}", result.parameters.len());
            println!("converged: {}", result.converged);
            if !result.converged {
                return Err(format!("UCC({rank}) did not converge").into());
            }
        }
        Command::Rhf { system } => {
            let started = Instant::now();
            let integrals = compute_ao_integrals(&system.molecule())?;
            let integral_time = started.elapsed();
            let rhf_started = Instant::now();
            let result = solve_rhf(&integrals, &RhfConfig::default())?;
            println!("system: {}", system.label());
            println!("atomic orbitals: {}", integrals.nao);
            println!("electrons: {}", integrals.nelec);
            println!("basis provenance: {}", integrals.basis_provenance);
            println!("coordinate unit: {}", integrals.coordinate_unit);
            println!("energy unit: {ENERGY_UNIT}");
            println!("nuclear repulsion: {:.15}", integrals.nuclear_repulsion);
            println!("RHF total energy: {:.15}", result.total_energy);
            println!("density RMS: {:.3e}", result.density_rms);
            println!("iterations: {}", result.iterations);
            println!("integral time: {:.3?}", integral_time);
            println!("RHF time: {:.3?}", rhf_started.elapsed());
            println!("converged: {}", result.converged);
            if !result.converged {
                return Err("RHF did not converge".into());
            }
        }
        Command::DirectIntegralsFci {
            system,
            residual_tolerance,
            max_iterations,
            max_subspace,
            parallel_blocks,
            parallel_memory_budget_gib,
            strict_parallel_memory,
        } => {
            let integral_started = Instant::now();
            let integrals = compute_ao_integrals(&system.molecule())?;
            let integral_time = integral_started.elapsed();
            let rhf_started = Instant::now();
            let rhf = solve_rhf(&integrals, &RhfConfig::default())?;
            let rhf_time = rhf_started.elapsed();
            if !rhf.converged {
                return Err("RHF did not converge".into());
            }
            let transform_started = Instant::now();
            let problem = transform_to_mo(&integrals, &rhf)?;
            let transform_time = transform_started.elapsed();
            let operator =
                DirectFciOperator::new(problem)?.with_execution_policy(execution_policy(
                    parallel_blocks,
                    parallel_memory_budget_gib,
                    strict_parallel_memory,
                )?)?;
            let mut initial = vec![0.0; operator.dimension()];
            let reference_index = operator
                .diagonal()
                .iter()
                .enumerate()
                .min_by(|(_, left), (_, right)| left.total_cmp(right))
                .map(|(index, _)| index)
                .ok_or("empty determinant basis")?;
            initial[reference_index] = 1.0;
            let fci_started = Instant::now();
            let result = lowest_eigenpair(
                &operator,
                &initial,
                &DavidsonConfig {
                    residual_tolerance,
                    energy_tolerance: residual_tolerance * 0.01,
                    max_iterations,
                    max_subspace,
                },
            )?;
            println!("system: {}", system.label());
            println!("atomic/molecular orbitals: {}", integrals.nao);
            println!("electrons: {}", integrals.nelec);
            println!("basis provenance: {}", integrals.basis_provenance);
            println!("coordinate unit: {}", integrals.coordinate_unit);
            println!("energy unit: {ENERGY_UNIT}");
            println!("determinants: {}", operator.dimension());
            println!("RHF total energy: {:.15}", rhf.total_energy);
            println!("FCI total energy: {:.15}", result.energy);
            println!("FCI residual norm: {:.3e}", result.residual_norm);
            println!("FCI iterations: {}", result.iterations);
            print_execution_preflight(&operator);
            println!("integral time: {:.3?}", integral_time);
            println!("RHF time: {:.3?}", rhf_time);
            println!("AO-to-MO time: {:.3?}", transform_time);
            println!("FCI time: {:.3?}", fci_started.elapsed());
            println!("converged: {}", result.converged);
            if !result.converged {
                return Err("direct-integrals FCI did not converge".into());
            }
        }
        Command::Benchmark {
            system,
            sources,
            memory_budget_gib,
            json_output,
        } => {
            let result = match system {
                BenchmarkSystem::H2oCcPvdz => run_h2o_cc_pvdz_benchmark(BoundedBenchmarkConfig {
                    sources,
                    memory_budget_gib,
                })?,
            };
            print_benchmark_result(&result);
            if let Some(path) = json_output {
                if let Some(parent) = path.parent()
                    && !parent.as_os_str().is_empty()
                {
                    fs::create_dir_all(parent)?;
                }
                fs::write(&path, serde_json::to_vec_pretty(&result)?)?;
                println!("JSON output: {}", path.display());
            }
        }
        Command::Verify {
            fcidump,
            reference,
            tolerance,
        } => {
            if !tolerance.is_finite() || tolerance <= 0.0 {
                return Err("tolerance must be finite and positive".into());
            }
            let bytes = fs::read(&fcidump)?;
            let actual_checksum = sha256_hex(&bytes);
            let reference = Reference::load(&reference)?;
            if actual_checksum != reference.fcidump_sha256 {
                return Err(format!(
                    "FCIDUMP checksum mismatch: expected {}, got {}",
                    reference.fcidump_sha256, actual_checksum
                )
                .into());
            }
            let dump = Fcidump::parse(std::str::from_utf8(&bytes)?)?;
            let rust_energy = ground_state_energy(&dump)?;
            let error = (rust_energy - reference.fci_energy).abs();
            println!("system: {}", reference.system);
            println!("Rust dense FCI: {:.15}", rust_energy);
            println!("PySCF FCI:      {:.15}", reference.fci_energy);
            println!("absolute error: {:.3e}", error);
            println!("tolerance:      {:.3e}", tolerance);
            if error > tolerance {
                return Err(format!("verification failed: {error:e} > {tolerance:e}").into());
            }
            println!("verification: PASS");
        }
    }
    Ok(())
}

fn print_benchmark_result(result: &BoundedBenchmarkResult) {
    println!("system: {}", result.system);
    println!("geometry: {}", result.geometry);
    println!("basis: {}", result.basis);
    println!("basis provenance: {}", result.basis_provenance);
    println!("coordinate unit: {}", result.coordinate_unit);
    println!("energy unit: {}", result.energy_unit);
    println!("all electrons: {}", result.all_electron);
    println!("point-group symmetry: {}", result.point_group_symmetry);
    println!("orbitals: {}", result.norb);
    println!("electrons: {}", result.nelec);
    println!("Nalpha/Nbeta: {}/{}", result.nalpha, result.nbeta);
    println!("alpha strings: {}", result.space.alpha_strings);
    println!("beta strings: {}", result.space.beta_strings);
    println!("determinants: {}", result.space.determinants);
    println!(
        "one dense CI vector: {:.6} GiB",
        result.space.vector_bytes as f64 / GIB
    );
    println!(
        "current Davidson minimum: {:.6} GiB",
        result.space.minimum_current_davidson_bytes as f64 / GIB
    );
    println!(
        "24-vector-pair Davidson subspace: {:.6} GiB",
        result.space.subspace_24_bytes as f64 / GIB
    );
    println!(
        "bounded benchmark estimate: {:.6} GiB",
        result.bounded_memory.conservative_peak_bytes as f64 / GIB
    );
    println!(
        "memory budget: {:.6} GiB",
        result.memory_budget_bytes as f64 / GIB
    );
    println!("RHF total energy: {:.15}", result.rhf_total_energy);
    println!("PySCF RHF reference: {:.15}", result.rhf_reference_energy);
    println!("RHF absolute error: {:.3e} Eh", result.rhf_absolute_error);
    println!("RHF iterations: {}", result.rhf_iterations);
    println!("RHF density RMS: {:.3e}", result.rhf_density_rms);
    println!(
        "integral time: {:.6} s",
        result.timings.ao_integrals_seconds
    );
    println!("RHF time: {:.6} s", result.timings.rhf_seconds);
    println!("AO-to-MO time: {:.6} s", result.timings.ao_to_mo_seconds);
    println!(
        "string/link time: {:.6} s",
        result.timings.link_tables_seconds
    );
    println!(
        "sparse-column time: {:.6} s",
        result.timings.sparse_columns_seconds
    );
    println!("sparse columns: {}", result.sparse_kernel.sources);
    println!(
        "raw Hamiltonian contributions: {}",
        result.sparse_kernel.raw_contributions
    );
    println!(
        "Hamiltonian contributions/s: {:.3e}",
        result.sparse_kernel.contributions_per_second
    );
    println!(
        "sparse-column checksum: {:.15}",
        result.sparse_kernel.checksum
    );
    println!("Rayon threads: {}", result.rayon_threads);
    println!("full FCI executed: {}", result.full_fci_executed);
}

const GIB: f64 = (1024_u64.pow(3)) as f64;

fn gib_to_bytes(gib: f64) -> Result<u64, Box<dyn std::error::Error>> {
    if !gib.is_finite() || gib <= 0.0 {
        return Err("memory budget must be finite and positive".into());
    }
    let bytes = gib * GIB;
    if bytes > u64::MAX as f64 {
        return Err("memory budget exceeds u64 byte range".into());
    }
    Ok(bytes.ceil() as u64)
}

fn davidson_resident_bytes(
    dimension: usize,
    max_subspace: usize,
    disk_workspace: bool,
) -> Result<u64, Box<dyn std::error::Error>> {
    let resident_vectors = if disk_workspace {
        7
    } else {
        max_subspace
            .checked_mul(2)
            .and_then(|value| value.checked_add(6))
            .ok_or("Davidson resident-vector count overflow")?
    };
    let bytes = dimension
        .checked_mul(resident_vectors)
        .and_then(|value| value.checked_mul(size_of::<f64>()))
        .ok_or("Davidson resident-byte estimate overflow")?;
    Ok(u64::try_from(bytes)?)
}

fn execution_policy(
    blocks: usize,
    memory_budget_gib: f64,
    strict_memory: bool,
) -> Result<ExecutionPolicy, Box<dyn std::error::Error>> {
    if blocks <= 1 {
        return Ok(ExecutionPolicy::Serial);
    }
    Ok(ExecutionPolicy::Parallel {
        blocks,
        memory_budget_bytes: gib_to_bytes(memory_budget_gib)?,
        allow_serial_fallback: !strict_memory,
    })
}

fn print_execution_preflight(operator: &DirectFciOperator) {
    let report = operator.execution_preflight();
    println!("requested sigma mode: {:?}", report.requested_mode);
    println!("effective sigma mode: {:?}", report.effective_mode);
    println!("sigma source blocks: {}", report.effective_blocks);
    println!(
        "parallel sigma workspace: {:.6} GiB",
        report.workspace_bytes as f64 / GIB
    );
    if let Some(reason) = report.fallback_reason {
        println!("parallel sigma fallback: {reason}");
    }
}

fn validate_hirata_context(
    table: &HirataTable2,
    reference: &Reference,
    operator: &DirectFciOperator,
) -> Result<(), Box<dyn std::error::Error>> {
    let problem = operator.problem();
    let basis_matches = reference
        .basis
        .as_deref()
        .is_some_and(|basis| basis.eq_ignore_ascii_case(&table.system.basis));
    let coordinate_unit_matches = reference
        .coordinate_unit
        .as_deref()
        .is_some_and(|unit| unit.eq_ignore_ascii_case("angstrom"));
    let context_matches = basis_matches
        && coordinate_unit_matches
        && reference.frozen_orbitals == table.system.frozen_orbitals
        && reference.number_of_active_molecular_orbitals
            == Some(table.system.active_spatial_orbitals)
        && reference.number_of_active_electrons == Some(table.system.active_electrons)
        && problem.norb == table.system.active_spatial_orbitals
        && problem.nelec == table.system.active_electrons
        && operator.dimension() == table.system.determinants
        && rounded_published_match(
            reference.fci_energy,
            table.system.fci_energy_printed,
            table.printed_decimals,
        );
    if !context_matches {
        return Err(
            "fixture settings do not match Hirata 2000 Table 2 equilibrium H2O/6-31G".into(),
        );
    }
    Ok(())
}
