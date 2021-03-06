/*++
  Copyright (c) 2020 Microsoft Corporation

  Module Name:

   sat_aig_cuts.h

  Abstract:
   
    Extract AIG definitions from clauses.
    Perform cut-set enumeration to identify equivalences.

    AIG extraction is incremental.
    It can be called repeatedly.
    Initially, a main aig node is inserted 
    (from initial clauses or the input 
    clausification in goal2sat).
    Then, auxiliary AIG nodes can be inserted
    by walking the current set of main and learned 
    clauses. AIG nodes with fewer arguments are preferred.
    
    

  Author:

    Nikolaj Bjorner 2020-01-02

  --*/

#pragma once

#include "sat/sat_cutset.h"
#include "sat/sat_types.h"

namespace sat {

    enum bool_op {
        var_op,
        and_op,
        ite_op,
        xor_op,
        no_op
    };

    inline std::ostream& operator<<(std::ostream& out, bool_op op) {
        switch (op) {
        case var_op: return out << "v";
        case and_op: return out << "&";
        case ite_op: return out << "?";
        case xor_op: return out << "^";
        default: return out << "";
        }
    }

    class aig_cuts {
    public:
        typedef std::function<void(literal_vector const&)> on_clause_t;

        struct config {
            unsigned m_max_cutset_size;
            unsigned m_max_aux;
            unsigned m_max_insertions;
            bool     m_full;
        config(): m_max_cutset_size(20), m_max_aux(5), m_max_insertions(20), m_full(false) {}
        };
    private:

        // encodes one of var, and, !and, xor, !xor, ite, !ite.
        class node {
            bool     m_sign;
            bool_op  m_op;
            unsigned m_size;
            unsigned m_offset;
        public:
            node(): 
                m_sign(false), m_op(no_op), m_size(UINT_MAX), m_offset(UINT_MAX) {}
            explicit node(unsigned v) : 
                m_sign(false), m_op(var_op), m_size(0), m_offset(v) {}
            explicit node(bool sign, bool_op op, unsigned nc, unsigned o) : 
                m_sign(sign), m_op(op), m_size(nc), m_offset(o) {}
            bool is_valid() const { return m_offset != UINT_MAX; }
            bool_op op() const { return m_op; }
            bool is_var() const { return m_op == var_op; }
            bool is_and() const { return m_op == and_op; }
            bool is_xor() const { return m_op == xor_op; }
            bool is_ite() const { return m_op == ite_op; }
            bool is_const() const { return is_and() && size() == 0; }
            unsigned var() const { SASSERT(is_var()); return m_offset; }
            bool sign() const { return m_sign; }
            unsigned size() const { return m_size; }
            unsigned offset() const { return m_offset; }
        };
        random_gen            m_rand;
        config                m_config;
        vector<svector<node>> m_aig;    
        literal_vector        m_literals;
        region                m_region;
        cut_set               m_cut_set1, m_cut_set2;
        vector<cut_set>       m_cuts;
        unsigned_vector       m_max_cutset_size;
        unsigned_vector       m_last_touched;
        unsigned              m_num_cut_calls;
        unsigned              m_num_cuts;
        svector<std::pair<bool_var, literal>> m_roots;
        unsigned              m_insertions;
        on_clause_t           m_on_clause_add, m_on_clause_del;
        cut_set::on_update_t  m_on_cut_add, m_on_cut_del;
        literal_vector        m_clause;

        bool is_touched(bool_var v, node const& n);
        bool is_touched(literal lit) const { return is_touched(lit.var()); }
        bool is_touched(bool_var v) const { return m_last_touched[v] + m_aig.size() >= m_num_cut_calls * m_aig.size(); }
        void reserve(unsigned v);
        bool insert_aux(unsigned v, node const& n);
        void init_cut_set(unsigned id);

        unsigned max_cutset_size(unsigned v) const { return v == UINT_MAX ? m_config.m_max_cutset_size : m_max_cutset_size[v]; }

        bool eq(node const& a, node const& b);

        unsigned_vector filter_valid_nodes() const;
        void augment(unsigned_vector const& ids);
        void augment(unsigned id, node const& n);
        void augment_ite(unsigned v, node const& n, cut_set& cs);
        void augment_aig0(unsigned v, node const& n, cut_set& cs);
        void augment_aig1(unsigned v, node const& n, cut_set& cs);
        void augment_aig2(unsigned v, node const& n, cut_set& cs);
        void augment_aigN(unsigned v, node const& n, cut_set& cs);

        bool insert_cut(unsigned v, cut const& c, cut_set& cs);

        void flush_roots();
        void flush_roots(literal_vector const& to_root, node& n);
        void flush_roots(literal_vector const& to_root, cut_set& cs);

        uint64_t eval(node const& n, svector<uint64_t> const& env) const;

        std::ostream& display(std::ostream& out, node const& n) const;

        literal child(node const& n, unsigned idx) const { SASSERT(!n.is_var()); SASSERT(idx < n.size()); return m_literals[n.offset() + idx]; }

        void on_node_add(unsigned v, node const& n);
        void on_node_del(unsigned v, node const& n);

        void evict(cut_set& cs, unsigned idx) { cs.evict(m_on_cut_del, idx); }
        void reset(cut_set& cs) { cs.reset(m_on_cut_del); }
        void push_back(cut_set& cs, cut const& c) { cs.push_back(m_on_cut_add, c); }
        void shrink(cut_set& cs, unsigned j) { cs.shrink(m_on_cut_del, j); }

        void cut2clauses(on_clause_t& on_clause, unsigned v, cut const& c);
        void node2def(on_clause_t& on_clause, node const& n, literal r);

        struct validator;
        void validate_cut(unsigned v, cut const& c);
        void validate_aig2(cut const& a, cut const& b, unsigned v, node const& n, cut const& c);
        void validate_aigN(unsigned v, node const& n, cut const& c); 

    public:

        aig_cuts();
        void add_var(unsigned v);
        void add_node(literal head, bool_op op, unsigned sz, literal const* args);
        void set_root(bool_var v, literal r);

        void set_on_clause_add(on_clause_t& on_clause_add);
        void set_on_clause_del(on_clause_t& on_clause_del);

        void inc_max_cutset_size(unsigned v) { m_max_cutset_size[v] += 10; touch(v); }

        vector<cut_set> const & operator()();
        unsigned num_cuts() const { return m_num_cuts; }

        void cut2def(on_clause_t& on_clause, cut const& c, literal r);

        void touch(bool_var v) { m_last_touched[v] = v + m_num_cut_calls * m_aig.size(); }

        svector<uint64_t> simulate(unsigned num_rounds);

        std::ostream& display(std::ostream& out) const;

    };

}


