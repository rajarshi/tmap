#include "minhash.hh"

Minhash::Minhash(unsigned int d, unsigned int seed, unsigned int sample_size) : d_(d), sample_size_(sample_size),
                                                                                perms_a_((uint32_t)0, d), perms_b_((uint32_t)0, d),
                                                                                rs_(sample_size), rs_2_(sample_size), ln_cs_(sample_size),
                                                                                cs_(sample_size),
                                                                                betas_(sample_size), betas_2_(sample_size)
{
    // Permutations for the standard minhash
    std::mt19937 rand;
    rand.seed(seed);
    
    std::uniform_int_distribution<std::mt19937::result_type> dist_a(1, max_hash_);
    std::uniform_int_distribution<std::mt19937::result_type> dist_b(0, max_hash_);
    
    std::vector<uint32_t> perms_a_tmp(d, 0);
    std::vector<uint32_t> perms_b_tmp(d, 0);

    for (unsigned int i = 0; i < d; i++)
    {
        uint32_t a = dist_a(rand);
        uint32_t b = dist_b(rand);

        while (std::find(perms_a_tmp.begin(), perms_a_tmp.end(), a) != perms_a_tmp.end())
        {
            a = dist_a(rand);
        }

        while (std::find(perms_b_tmp.begin(), perms_b_tmp.end(), a) != perms_b_tmp.end())
        {
            b = dist_a(rand);
        }

        perms_a_tmp[i] = a;
        perms_b_tmp[i] = b;

        perms_a_[i] = a;
        perms_b_[i] = b;
    }

    // This is for the weighted minhash
    std::default_random_engine rand_gamma;
    rand_gamma.seed(seed);
    std::default_random_engine rand_gamma_2;
    rand_gamma_2.seed(seed);
    std::default_random_engine rand_gamma_3;
    rand_gamma_3.seed(seed);
    std::gamma_distribution<float> gamma_dist(2.0, 1.0);
    std::gamma_distribution<float> gamma_dist_2(2.0, 1.0);
    std::gamma_distribution<float> gamma_dist_3(2.0, 1.0);
    
    std::uniform_real_distribution<float> dist_beta(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_beta_2(0.0f, 1.0f);

    for (unsigned int i = 0; i < sample_size_; i++)
    {
        rs_[i] = std::valarray<float>(0.0f, d_);
        rs_2_[i] = std::valarray<float>(0.0f, d_);
        ln_cs_[i] = std::valarray<float>(0.0f, d_);
        cs_[i] = std::valarray<float>(0.0f, d_);
        betas_[i] = std::valarray<float>(0.0f, d_);
        betas_2_[i] = std::valarray<float>(0.0f, d_);

        for (unsigned int j = 0; j < d_; j++)
        {
            rs_[i][j] = gamma_dist(rand_gamma);
            rs_2_[i][j] = gamma_dist_2(rand_gamma_2);
            ln_cs_[i][j] = std::log(gamma_dist_3(rand_gamma_3));
            cs_[i][j] = gamma_dist_3(rand_gamma_3);
            betas_[i][j] = dist_beta(rand);
            betas_2_[i][j] = dist_beta(rand);
        }
    }
}

std::vector<uint32_t> Minhash::FromBinaryArray(std::vector<uint8_t> &vec)
{
    std::valarray<uint32_t> mh(max_hash_, d_);

    for (uint32_t i = 0; i < vec.size(); i++)
    {
        if (vec[i] == 0)
            continue;
        
        std::valarray<uint32_t> tmp = ((perms_a_ * i + perms_b_) % prime_) % max_hash_;

        for (size_t j = 0; j < mh.size(); j++)
        {
            mh[j] = std::min(tmp[j], mh[j]);
        }
    }

    return std::vector<uint32_t>(std::begin(mh), std::end(mh));
}

std::vector<std::vector<uint32_t>> Minhash::BatchFromBinaryArray(std::vector<std::vector<uint8_t>> &vecs)
{
    std::vector<std::vector<uint32_t>> results(vecs.size());

    #pragma omp parallel for
    for (size_t i = 0; i < vecs.size(); i++)
        results[i] = FromBinaryArray(vecs[i]);

    return results;
}

std::vector<uint32_t> Minhash::FromSparseBinaryArray(std::vector<uint32_t> &vec)
{
    std::valarray<uint32_t> mh(max_hash_, d_);

    for (uint32_t i = 0; i < vec.size(); i++)
    {        
        std::valarray<uint32_t> tmp = ((perms_a_ * vec[i] + perms_b_) % prime_) % max_hash_;

        for (size_t j = 0; j < mh.size(); j++)
        {
            mh[j] = std::min(tmp[j], mh[j]);
        }
    }

    return std::vector<uint32_t>(std::begin(mh), std::end(mh));
}

std::vector<std::vector<uint32_t>> Minhash::BatchFromSparseBinaryArray(std::vector<std::vector<uint32_t>> &vecs)
{
    std::vector<std::vector<uint32_t>> results(vecs.size());

    #pragma omp parallel for
    for (size_t i = 0; i < vecs.size(); i++)
        results[i] = FromSparseBinaryArray(vecs[i]);

    return results;
}

std::vector<uint32_t> Minhash::FromStringArray(std::vector<std::string> &vec)
{
    std::valarray<uint32_t> mh(max_hash_, d_);
    sha1::SHA1 s;

    for (uint32_t i = 0; i < vec.size(); i++)
    {
        s.processBytes(vec[i].c_str(), vec[i].size());
        uint32_t digest[5];
        s.getDigest(digest);

        std::valarray<uint32_t> tmp = ((perms_a_ * digest[0] + perms_b_) % prime_) % max_hash_;

        for (size_t j = 0; j < mh.size(); j++)
            mh[j] = std::min(tmp[j], mh[j]); 
    }

    return std::vector<uint32_t>(std::begin(mh), std::end(mh));
}

std::vector<std::vector<uint32_t>> Minhash::BatchFromStringArray(std::vector<std::vector<std::string>> &vecs)
{
    std::vector<std::vector<uint32_t>> results(vecs.size());

    #pragma omp parallel for
    for (size_t i = 0; i < vecs.size(); i++)
        results[i] = FromStringArray(vecs[i]);

    return results;
}

std::vector<uint32_t> Minhash::FromWeightArray(std::vector<float> &vec)
{
    if (vec.size() != d_)
        throw std::invalid_argument("The length of the weighted input vector has to be the same length as the dimenstionality with which Minhash was initialized");

    std::vector<uint32_t> mh(sample_size_ * 2);

    std::vector<float> vals;
    std::vector<size_t> indices;
    std::valarray<bool> mask(vec.size());

    for (size_t i = 0; i < vec.size(); i++)
    {
        if (vec[i] > 0)
        {
            indices.emplace_back(i);

            if (vec[i] > 0)
                vals.emplace_back(vec[i]);

            mask[i] = true;
        }
        else 
            mask[i] = false;
    }      

    std::valarray<float> vals_log(vals.data(), vals.size());

    // ICWS

    for (unsigned int s = 0; s < sample_size_; s++)
    {
        std::valarray<float> rs = rs_[s][mask];
        std::valarray<float> betas = betas_[s][mask];
        std::valarray<float> ln_cs = ln_cs_[s][mask];

        std::valarray<float> t = (vals_log / rs) + betas;
        t = t.apply([](float x) { return std::floor(x); });

        std::valarray<float> ln_y = (t - betas) * rs;
        std::valarray<float> ln_a = ln_cs - ln_y - rs;

        uint32_t k_star = std::distance(std::begin(ln_a), std::min_element(std::begin(ln_a), std::end(ln_a)));

        mh[2 * s] = indices[k_star];
        mh[2 * s + 1] = (uint32_t)t[k_star];
    }

    // I2CWS

    // for (unsigned int i = 0; i < sample_size_; i++)
    // {
    //     std::valarray<float> rs = rs_[i][mask];
    //     std::valarray<float> rs_2 = rs_2_[i][mask];
    //     std::valarray<float> betas = betas_[i][mask];
    //     std::valarray<float> betas_2 = betas_2_[i][mask];
    //     std::valarray<float> cs = cs_[i][mask];

    //     std::valarray<float> t = (vals_log / rs_2) + betas_2;
    //     t = t.apply([](float x) { return std::floor(x); });

    //     std::valarray<float> z = std::exp(rs_2 * (t - betas_2 + 1));
    //     std::valarray<float> a = cs / z;

    //     uint32_t k_star = std::distance(std::begin(a), std::min_element(std::begin(a), std::end(a)));
    //     uint32_t t_k = std::floor(vals_log[k_star] / rs[k_star] + betas[k_star]);

    //     mh[2 * i] = indices[k_star];
    //     mh[2 * i + 1] = t_k;
    // }

    return mh;
}

std::vector<std::vector<uint32_t>> Minhash::BatchFromWeightArray(std::vector<std::vector<float>> &vecs)
{
    std::vector<std::vector<uint32_t>> results(vecs.size());

    #pragma omp parallel for
    for (size_t i = 0; i < vecs.size(); i++)
        results[i] = FromWeightArray(vecs[i]);

    return results;
}

float Minhash::GetDistance(std::vector<uint32_t> &vec_a, std::vector<uint32_t> &vec_b)
{
    float intersect = 0;
    size_t length = vec_a.size();

    for (unsigned int i = 0; i < length; i++)
        if (vec_a[i] == vec_b[i])
            intersect++;

    return 1.0f - intersect / length;
}

float Minhash::GetWeightedDistance(std::vector<uint32_t> &vec_a, std::vector<uint32_t> &vec_b)
{
    float intersect = 0;
    size_t length = vec_a.size();

    for (unsigned int i = 0; i < length; i += 2)
        if (vec_a[i] == vec_b[i] && vec_a[i + 1] == vec_b[i + 1])
            intersect++;

    return 1.0f - 2.0f * intersect / length;
}