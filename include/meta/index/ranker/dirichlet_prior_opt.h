/**
 * @file dirichlet_prior_opt.h
 * @author Aleksey Marashov, Kolomiets Maxim
 *
 * All files in META are released under the MIT license. For more details,
 * consult the file LICENSE in the root of the project.
 */

#ifndef META_DIRICHLET_PRIOR_OPT_H_
#define META_DIRICHLET_PRIOR_OPT_H_

#include "meta/index/ranker/dirichlet_prior.h"

#include <cmath>

namespace meta
{
namespace index
{

// # TODO: choose template type instead of long
typedef long count_d;

struct docs_data
{
    // general info

    const inverted_index& idx;
    /// ids of all documents
    std::vector<doc_id> doc_ids;
    /// ids of all terms
    std::vector<term_id> term_ids;
    /// total size of documents
    count_d ref_size;
    /// C_.(n)
    std::map<count_d, count_d> docs_counts;
    /// C_k(n)
    std::map<term_id, std::map<count_d, count_d>> terms_docs_counts;
    /// vector alpha_m
    std::map<term_id, double> alpha_m;

    /**
     * Constructor to initialize most elements.
     * @param p_idx The index that is being used
     * @param p_doc_ids ids of all docs
     * @param p_term_ids ids of all terms
     */
    docs_data(const inverted_index& p_idx, std::vector<doc_id> p_doc_ids, std::vector<term_id> p_term_ids, count_d p_ref_size,
              std::map<count_d, count_d> p_docs_counts, std::map<term_id, std::map<count_d, count_d>> p_terms_docs_counts,
              std::map<term_id, double> p_alpha_m)
        : idx(p_idx), // gcc no non-const ref init from brace init list
          doc_ids{p_doc_ids},
          term_ids{p_term_ids},
          ref_size{p_ref_size},
          docs_counts{p_docs_counts},
          terms_docs_counts{p_terms_docs_counts},
          alpha_m{p_alpha_m}
    {
        /* nothing */
    }
};


/**
 * Implements Bayesian smoothing with a Dirichlet prior.
 *
 * Required config parameters:
 * ~~~toml
 * [ranker]
 * method = "dirichlet-prior"
 * ~~~
 *
 * Optional config parameters:
 * ~~~toml
 * mu = 2000.0
 * ~~~
 */
class dirichlet_prior_opt : public dirichlet_prior{
public:
    dirichlet_prior_opt(float mu) : dirichlet_prior(mu) { }

    dirichlet_prior_opt(std::istream& in) : dirichlet_prior(in) { }

    template <class ForwardIterator>
    std::vector<search_result> score(inverted_index& idx, ForwardIterator begin,
                                     ForwardIterator end,
                                     uint64_t num_results = 10)
    {
        this->optimize_mu(idx);

        return ranker::score(idx, begin, end, num_results);
    }

    std::map<term_id, double> get_optimized_mu(const inverted_index& idx, float eps, int max_iter) {
        return optimize_mu(idx, eps, max_iter);
    }

protected:
    inline double get_alpha(std::map<term_id, double> alpha_m){
        double alpha = 0;

        for (auto alpha_m_k: alpha_m){
            alpha += alpha_m_k.second;
        }

        return alpha;
    }

private:
    std::map<term_id, double> optimize_mu(const inverted_index& idx, float eps=1e-6, int max_iter=10000) {
        // parse idx and extract what we need

        auto docs_ids = idx.docs();
        auto terms_ids = idx.terms();

        // calculate ref_size
        count_d ref_size = 0;
        for (auto& id : docs_ids)
            ref_size += idx.doc_size(id);

        // calculate C_.(n) and C_k(n)
        std::map<count_d, count_d> docs_counts;
        std::map<term_id, std::map<count_d, count_d>> terms_docs_counts;

        long doc_size, doc_term_freq;
        for (auto d_id: docs_ids){
            doc_size = idx.doc_size(d_id);

            //// increase number of docs with the given size (C_.(n))
            docs_counts[doc_size] += 1;

            for (auto t_id: terms_ids){
                doc_term_freq = idx.term_freq(t_id, d_id);

                //// increase number of docs with the given count of word t_id (C_k(n))
                terms_docs_counts[t_id][doc_term_freq] += 1;
            }
        }

        // fill start vector alpha_m
        std::map<term_id, double> alpha_m;

        for (auto t_id: terms_ids){
            alpha_m[t_id] = idx.total_num_occurences(t_id) * default_mu;
            alpha_m[t_id] /= (double)ref_size;
        }

        // create docs_data
        docs_data dd{idx, docs_ids, terms_ids, ref_size, docs_counts, terms_docs_counts, alpha_m};

        // call optimizer
        return optimize_mu(dd, eps, max_iter);
    }

    virtual std::map<term_id, double> optimize_mu(docs_data& dd, float eps, int max_iter) = 0;
};

class dirichlet_digamma_rec: public dirichlet_prior_opt{
public:
    const static util::string_view id;

    /**
     * @param mu
     */
    dirichlet_digamma_rec(float mu = default_mu);

    /**
     * Loads a dirichlet_prior ranker from a stream.
     * @param in The stream to read from
     */
    dirichlet_digamma_rec(std::istream& in);

    void save(std::ostream& out) const override;
private:
    std::map<term_id, double> optimize_mu(docs_data& dd, float eps, int max_iter) override;

};

class dirichlet_log_approx: public dirichlet_prior_opt{
public:
    const static util::string_view id;

    /**
     * @param mu
     */
    dirichlet_log_approx(float mu = default_mu);

    /**
     * Loads a dirichlet_prior ranker from a stream.
     * @param in The stream to read from
     */
    dirichlet_log_approx(std::istream& in);

    void save(std::ostream& out) const override;
private:
    std::map<term_id, double> optimize_mu(docs_data& dd, float eps, int max_iter) override;
};

class dirichlet_mackay_peto: public dirichlet_prior_opt{
public:
    const static util::string_view id;

    /**
     * @param mu
     */
    dirichlet_mackay_peto(float mu = default_mu);

    /**
     * Loads a dirichlet_prior ranker from a stream.
     * @param in The stream to read from
     */
    dirichlet_mackay_peto(std::istream& in);

    void save(std::ostream& out) const override;
private:
    std::map<term_id, double> optimize_mu(docs_data& dd, float eps, int max_iter) override;
};

/**
 * Specialization of the factory method used to create dirichlet_digamma_rec
 * rankers.
 */
template <>
std::unique_ptr<ranker> make_ranker<dirichlet_digamma_rec>(const cpptoml::table&);

/**
 * Specialization of the factory method used to create dirichlet_log_approx
 * rankers.
 */
template <>
std::unique_ptr<ranker> make_ranker<dirichlet_log_approx>(const cpptoml::table&);

/**
 * Specialization of the factory method used to create dirichlet_mackay_peto
 * rankers.
 */
template <>
std::unique_ptr<ranker> make_ranker<dirichlet_mackay_peto>(const cpptoml::table&);
}
}
#endif
