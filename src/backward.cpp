/*
 Arjun

 Copyright (c) 2019, Mate Soos and Kuldeep S. Meel. All rights reserved.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#include "common.h"

void Common::fill_assumptions_backward(
    vector<Lit>& assumptions,
    vector<uint32_t>& unknown,
    const vector<char>& unknown_set,
    const vector<uint32_t>& indep)
{
    if (conf.verb > 5) {
        cout << "Filling assumps BEGIN" << endl;
    }
    assumptions.clear();

    //Add known independent as assumptions
    for(const auto& var: indep) {
        assert(var < orig_num_vars);

        uint32_t indic = var_to_indic[var];
        assert(indic != var_Undef);
        assumptions.push_back(Lit(indic, true));
        if (conf.verb > 5) {
            cout << "Filled assump with indep: " << var << endl;
        }
    }

    //Add unknown as assumptions, clean "unknown"
    uint32_t j = 0;
    for(uint32_t i = 0; i < unknown.size(); i++) {
        uint32_t var = unknown[i];
        if (unknown_set[var] == 0) {
            continue;
        } else {
            unknown[j++] = var;
        }
        if (conf.verb > 5) {
            cout << "Filled assump with unknown: " << var << endl;
        }

        assert(var < orig_num_vars);
        uint32_t indic = var_to_indic[var];
        assert(indic != var_Undef);
        assumptions.push_back(Lit(indic, true));
    }
    unknown.resize(j);
    if (conf.verb > 5) {
        cout << "Filling assumps END, total assumps size: " << assumptions.size() << endl;
    }
}

bool Common::backward_round(
    uint32_t max_iters)
{
    for(const auto& x: seen) {
        assert(x == 0);
    }

    double start_round_time = cpuTimeTotal();
    //start with empty independent set
    vector<uint32_t> indep;

    //Initially, all of samping_set is unknown
    vector<uint32_t> unknown;
    vector<char> unknown_set;
    unknown_set.resize(orig_num_vars, 0);
    for(const auto& x: *sampling_set) {
        assert(unknown_set[x] == 0 && "No var should be in 'sampling_set' twice!");
        unknown.push_back(x);
        unknown_set[x] = 1;
    }
    std::sort(unknown.begin(), unknown.end(), IncidenceSorter<uint32_t>(incidence));
    cout << "c [mis] Start unknown size: " << unknown.size() << endl;

    vector<Lit> assumptions;
    uint32_t iter = 0;
    uint32_t not_indep = 0;

    double myTime = cpuTime();

    //Calc mod:
    uint32_t mod = 1;
    if ((sampling_set->size()) > 20 ) {
        uint32_t will_do_iters = sampling_set->size();
        uint32_t want_printed = 30;
        mod = will_do_iters/want_printed;
        mod = std::max<int>(mod, 1);
    }


    uint32_t ret_false = 0;
    uint32_t ret_true = 0;
    uint32_t ret_undef = 0;
    bool quick_pop_ok = false;
    uint32_t backbone_calls = 0;
    uint32_t backbone_max = 0;
    uint32_t backbone_tot = 0;
    vector<uint32_t> non_indep_vars;
    while(iter < max_iters) {
        uint32_t test_var = var_Undef;
        if (quick_pop_ok) {
            //Remove 2 last
            assumptions.pop_back();
            assumptions.pop_back();

            //No more left, try again with full
            if (assumptions.empty()) {
                break;
            }

            uint32_t ass_var = assumptions[assumptions.size()-1].var();
            assumptions.pop_back();
            assert(ass_var < indic_to_var.size());
            test_var = indic_to_var[ass_var];
            assert(test_var != var_Undef);
            assert(test_var < orig_num_vars);

            //something is messed up
            if (!unknown_set[test_var]) {
                quick_pop_ok = false;
                continue;
            }
            uint32_t last_unkn = unknown[unknown.size()-1];
            assert(last_unkn == test_var);
            unknown.pop_back();
        } else {
            while(!unknown.empty()) {
                uint32_t var = unknown[unknown.size()-1];
                if (unknown_set[var]) {
                    test_var = var;
                    unknown.pop_back();
                    break;
                } else {
                    unknown.pop_back();
                }
            }

            if (test_var == var_Undef) {
                //we are done, backward is finished
                break;
            }
        }
        assert(test_var < orig_num_vars);
        assert(unknown_set[test_var] == 1);
        unknown_set[test_var] = 0;
//         cout << "Testing: " << test_var << endl;

        //Assumption filling
        assert(test_var != var_Undef);
        if (!quick_pop_ok) {
            fill_assumptions_backward(assumptions, unknown, unknown_set, indep);
        }
        assumptions.push_back(Lit(test_var, false));
        assumptions.push_back(Lit(test_var + orig_num_vars, true));

        solver->set_no_confl_needed();

        lbool ret = l_Undef;
        solver->set_max_confl(conf.backw_max_confl);
        if (!conf.backbone) {
            ret = solver->solve(&assumptions);
        } else {
            BackBoneData b;
            b._assumptions = &assumptions;
            b.indic_to_var  = &indic_to_var;
            b.orig_num_vars = orig_num_vars;
            b.non_indep_vars = &non_indep_vars;
            b.indep_size = indep.size();
            b.backbone_on = true;
            b.backbone_test_var = &test_var;

            //solver->set_max_confl(conf.backw_max_confl);
            backbone_calls++;
            if (conf.verb > 5) {
                cout << "test var is: " << test_var << endl;
                cout << "find_backbone BEGIN " << endl;
            }
            non_indep_vars.clear();
            ret = solver->find_backbone(b);
            assert(ret != l_False);

//             cout
//             << "non_indep_vars.size(): " << non_indep_vars.size()
//             << " ret: " << ret
//             << " test_var: " << test_var
//             << endl;
            backbone_tot += non_indep_vars.size();
            backbone_max = std::max<uint32_t>(non_indep_vars.size(), backbone_max);
            for(uint32_t i = 0; i < non_indep_vars.size(); i ++) {
                uint32_t var = non_indep_vars[i];
                assert(var < orig_num_vars);
//                 cout << "backbone indep var: " << var << endl;
                if (i == 0) {
                    assert(unknown_set[var] == 0);
                } else {
                    assert(unknown_set[var] == 1);
                    unknown_set[var] = 0;
                }
                not_indep++;
            }
            quick_pop_ok = false;

            //We have finished it all off
            if (test_var == var_Undef) {
                continue;
            }
            unknown_set[test_var] = 0;
        }
//         cout << "Testing ret: " << ret << endl;
        if (ret == l_False) {
            ret_false++;
        } else if (ret == l_True) {
            ret_true++;
        } else if (ret == l_Undef) {
            ret_undef++;
        }

        assert(unknown_set[test_var] == 0);
        if (ret == l_Undef) {
            //Timed out, we'll treat is as unknown
            quick_pop_ok = false;
            assert(test_var < orig_num_vars);
            indep.push_back(test_var);
        } else if (ret == l_True) {
            //Independent
            quick_pop_ok = false;
            indep.push_back(test_var);
        } else if (ret == l_False) {
            //not independent
            //i.e. given that all in indep+unkown is equivalent, it's not possible that a1 != b1
            not_indep++;
            quick_pop_ok = true;
        }

        if (iter % mod == (mod-1)) {
            //remove_definable_by_gates();
            //solver->remove_and_clean_all();
            cout
            << "c [mis] iter: " << std::setw(5) << iter;
            if (mod == 1) {
                cout
                << " ret: " << std::setw(8) << ret;
            } else {
                cout
                << " T/F/U: ";
                std::stringstream ss;
                ss << ret_true << "/" << ret_false << "/" << ret_undef;
                cout << std::setw(10) << std::left << ss.str() << std::right;
                ret_true = 0;
                ret_false = 0;
                ret_undef = 0;
            }
            cout
            << " by: " << std::setw(3) << 1
            << " U: " << std::setw(7) << unknown.size()
            << " I: " << std::setw(7) << indep.size()
            << " N: " << std::setw(7) << not_indep
            ;
            if (conf.backbone) {
                cout << " backb avg:" << std::setprecision(1) << std::setw(7)
                << (double)backbone_tot/(double)backbone_calls
                << " backb max:" << std::setw(7) << backbone_max;
            }
            cout << " T: "
            << std::setprecision(2) << std::fixed << (cpuTime() - myTime)
            << endl;
            myTime = cpuTime();
            backbone_tot = 0;
            backbone_calls = 0;
            backbone_max = 0;
        }
        iter++;

        if (iter % 500 == 499) {
            update_sampling_set(unknown, unknown_set, indep);
        }
    }
    update_sampling_set(unknown, unknown_set, indep);
    cout << "c [mis] backward round finished T: "
    << std::setprecision(2) << std::fixed << (cpuTime() - start_round_time)
    << endl;
    solver->print_stats();

    return iter < max_iters;
}
