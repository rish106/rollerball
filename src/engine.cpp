#include <algorithm>
#include <chrono>
#include <iostream>
#include <climits>
#include <unordered_map>
#include <unordered_set>

using namespace std;

#include "board.hpp"
#include "engine.hpp"

const int SEARCH_DEPTH = 5;

const int PAWN_WEIGHT = 150;
const int ROOK_WEIGHT = 600;
const int BISHOP_WEIGHT = 400;
const int KING_WEIGHT = 1500;
const int CHECK_WEIGHT = 99;
const int STALEMATE_WEIGHT = 2000;

const int ATTACKING_FACTOR = 6;
const int DEFENDING_FACTOR = 4;

int nodes_visited;
int curr_player = -1;

int PLAYER_WEIGHTS[6] = {ROOK_WEIGHT, ROOK_WEIGHT, KING_WEIGHT, BISHOP_WEIGHT, PAWN_WEIGHT, PAWN_WEIGHT};
int OPPONENT_WEIGHTS[6] = {ROOK_WEIGHT, ROOK_WEIGHT, KING_WEIGHT, BISHOP_WEIGHT, PAWN_WEIGHT, PAWN_WEIGHT};

unordered_map<U8, int> quadrants;
U8 quad_points[4] = {pos(1, 1), pos(1, 5), pos(5, 5), pos(5, 1)};

unordered_map<string, int> previous_board_occurences;

struct Evaluation {
    int piece_weight    = 0;
    int promo           = 0;
    int check           = 0;
    int develop         = 0;
    int king_distance   = 0;
    int total           = 0;

    bool operator>(Evaluation& other_eval) {
        return total > other_eval.total;
    }

    static bool compare(const Evaluation& x, const Evaluation& y) {
        return x.total < y.total;
    }

    void reset() {
        piece_weight    = 0;
        promo           = 0;
        check           = 0;
        develop         = 0;
        king_distance   = 0;
        total           = 0;
    }

    void update_total() {
        total = 0;
        total += piece_weight;
        total += promo;
        total += check;
        total += develop;
        total += king_distance;
    }

    void print() {
        cout << "piece weight   " << piece_weight << '\n';
        cout << "promo          " << promo << '\n';
        cout << "check          " << check << '\n';
        cout << "develop        " << develop << '\n';
        cout << "king distance  " << king_distance << '\n';
        cout << "total          " << total << '\n';
    }
};

void flip_player(Board& b) {
    b.data.player_to_play = (PlayerColor)(b.data.player_to_play ^ (WHITE | BLACK));
}

void init_quadrant_map() {
    quadrants[pos(0, 0)] = quadrants[pos(5, 0)] = 0;
    for (int x = 1; x <= 4; x++) {
        for (int y = 0; y <= 1; y++) {
            quadrants[pos(x, y)] = 0;
        }
    }
    quadrants[pos(0, 1)] = quadrants[pos(0, 6)] = 1;
    for (int x = 0; x <= 1; x++) {
        for (int y = 2; y <= 5; y++) {
            quadrants[pos(x, y)] = 1;
        }
    }
    quadrants[pos(1, 6)] = quadrants[pos(6, 6)] = 2;
    for (int x = 2; x <= 5; x++) {
        for (int y = 5; y <= 6; y++) {
            quadrants[pos(x, y)] = 2;
        }
    }
    quadrants[pos(6, 0)] = quadrants[pos(6, 5)] = 3;
    for (int x = 5; x <= 6; x++) {
        for (int y = 1; y <= 4; y++) {
            quadrants[pos(x, y)] = 3;
        }
    }
}

int get_distance(U8 initial_pos, U8 final_pos) {
    int distance = 0;
    U8 distance_x, distance_y;
    U8 initial_quad = quadrants[initial_pos];
    U8 final_quad = quadrants[final_pos];
    U8 initial_x = getx(initial_pos);
    U8 initial_y = gety(initial_pos);
    U8 final_x = getx(final_pos);
    U8 final_y = gety(final_pos);
    if (initial_quad == final_quad) {
        U8 initial_coordinate = (initial_quad % 2 == 0 ? initial_x : initial_y);
        U8 final_coordinate = (initial_quad % 2 == 0 ? final_x : final_y);
        if ((((initial_quad == 1 || initial_quad == 2) && initial_coordinate >= final_coordinate) || (initial_quad == 0 || initial_quad == 3) && initial_coordinate <= final_coordinate)) {
            distance += 12;
            U8 prev_quad = (final_quad + 3) % 4;
            U8 special_point = quad_points[prev_quad];
            distance_x = abs(final_x - getx(special_point));
            distance_y = abs(final_y - gety(special_point));
            distance += max(distance_x, distance_y);
            special_point = quad_points[initial_quad];
            distance_x = abs(initial_x - getx(special_point));
            distance_y = abs(initial_y - gety(special_point));
            distance += max(distance_x, distance_y);
        } else {
            distance += final_coordinate - initial_coordinate;
        }
    } else {
        U8 quad_diff = (final_quad > initial_quad ? final_quad - initial_quad : 4 + final_quad - initial_quad);
        distance += 4 * (quad_diff - 1);
        U8 prev_quad = (final_quad + 3) % 4;
        U8 special_point = quad_points[prev_quad];
        distance_x = abs(final_x - getx(special_point));
        distance_y = abs(final_y - gety(special_point));
        distance += max(distance_x, distance_y);
        special_point = quad_points[initial_quad];
        distance_x = abs(initial_x - getx(special_point));
        distance_y = abs(initial_y - gety(special_point));
        distance += max(distance_x, distance_y);
    }
    return distance;
}

Evaluation eval(Board& b) {

    U8 white_pieces[6] = {b.data.w_rook_ws, b.data.w_rook_bs, b.data.w_king, b.data.w_bishop, b.data.w_pawn_ws, b.data.w_pawn_bs};
    U8 black_pieces[6] = {b.data.b_rook_ws, b.data.b_rook_bs, b.data.b_king, b.data.b_bishop, b.data.b_pawn_ws, b.data.b_pawn_bs};

    U8* player_pieces = (curr_player == WHITE ? white_pieces : black_pieces);
    U8* opponent_pieces = (curr_player == WHITE ? black_pieces : white_pieces);

    U8 player_promo = (curr_player == WHITE ? pos(4, 5) : pos(2, 0));
    U8 opponent_promo = (curr_player == WHITE ? pos(2, 0) : pos(4, 5));
    int player_promo_x = getx(player_promo);
    int player_promo_y = gety(player_promo);
    int opponent_promo_x = (curr_player == WHITE ? 2 : 4);
    int opponent_promo_y = (curr_player == WHITE ? 0 : 5);

    U8 player_develop_square = (curr_player == WHITE ? pos(1, 5) : pos(5, 1));
    // U8 opponent_develop_square = (curr_player == WHITE ? pos(5, 1) : pos(1, 5));
    int player_king_x = getx(player_pieces[2]);
    int player_king_y = gety(player_pieces[2]);
    int opponent_king_x = getx(opponent_pieces[2]);
    int opponent_king_y = gety(opponent_pieces[2]);

    Evaluation score;

    // unordered_set<U16> player_moves, opponent_moves;
    // (curr_player == b.data.player_to_play ? player_moves : opponent_moves) = b.get_legal_moves();
    // flip_player(b);
    // (curr_player == b.data.player_to_play ? player_moves : opponent_moves) = b.get_legal_moves();
    // flip_player(b);

    for (int i = 4; i < 6; i++) {
        if (b.data.board_0[player_pieces[i]] & ROOK) {
            PLAYER_WEIGHTS[i] = ROOK_WEIGHT;
        } else if (b.data.board_0[player_pieces[i]] & BISHOP) {
            PLAYER_WEIGHTS[i] = BISHOP_WEIGHT;
        } else {
            PLAYER_WEIGHTS[i] = PAWN_WEIGHT;
        }
        if (b.data.board_0[opponent_pieces[i]] & ROOK) {
            OPPONENT_WEIGHTS[i] = ROOK_WEIGHT;
        } else if (b.data.board_0[opponent_pieces[i]] & BISHOP) {
            OPPONENT_WEIGHTS[i] = BISHOP_WEIGHT;
        } else {
            OPPONENT_WEIGHTS[i] = PAWN_WEIGHT;
        }
    }

    auto add_piece_score = [&]() {
        for (int i = 0; i < 6; i++) {
            if (player_pieces[i] != DEAD) {
                score.piece_weight += PLAYER_WEIGHTS[i];
            }
            if (opponent_pieces[i] != DEAD) {
                score.piece_weight -= OPPONENT_WEIGHTS[i];
            }
        }
    };

    auto subtract_check_score = [&]() {
        if (b.in_check()) {
            if (b.get_legal_moves().empty()) {
                score.reset();
                score.check = (b.data.player_to_play == curr_player ? INT_MIN : INT_MAX);
            } else {
                score.check += (b.data.player_to_play == curr_player ? -1 : 1) * CHECK_WEIGHT;
            }
        }
    };

    auto calc_promo_score = [&](U8* pieces, U8 promo_pos) {
        int promo_score = 0;
        U8 promo_pos_y = gety(promo_pos);
        U8 piece_y;
        int distance_y, pawn_distance;
        int min_promo = INT_MAX;
        int max_promo = INT_MIN;
        for (int i = 4; i < 6; i++) {
            if (pieces[i] == DEAD || !(b.data.board_0[pieces[i]] & PAWN)) {
                continue;
            }
            piece_y = gety(pieces[i]);
            distance_y = min(abs(piece_y - promo_pos_y), abs(piece_y - promo_pos_y - 1));
            pawn_distance = get_distance(pieces[i], promo_pos);
            if (distance_y <= 1) {
                promo_score = 250 / (1 + pawn_distance);
            } else if (distance_y <= 3){
                promo_score = 120 / (1 + pawn_distance);
            } else {
                promo_score = 100 / (1 + pawn_distance);
            }
            min_promo = min(promo_score, min_promo);
            max_promo = max(promo_score, max_promo);
        }
        if (min_promo != INT_MAX) {
            promo_score = (max_promo + 2 * min_promo) / 3;
        }
        return promo_score;
    };

    auto add_promo_score = [&]() {
        score.promo += calc_promo_score(player_pieces, player_promo);
        score.promo -= calc_promo_score(opponent_pieces, opponent_promo);
    };

    auto add_king_distance_score = [&]() {
        for (int i = 0; i < 6; i++) {
            if (player_pieces[i] == DEAD || i == 2) {
                continue;
            }
            int player_piece_x = getx(player_pieces[i]);
            int player_piece_y = gety(player_pieces[i]);
            int distance_x = abs(opponent_king_x - player_piece_x);
            int distance_y = abs(opponent_king_y - player_piece_y);
            score.king_distance += PLAYER_WEIGHTS[i] / (20 + 10 * (distance_y + distance_x));
        }
        // for (int i = 0; i < 6; i++) {
        //     if (opponent_pieces[i] == DEAD) {
        //         continue;
        //     }
        //     int opponent_piece_x = getx(opponent_pieces[i]);
        //     int opponent_piece_y = gety(opponent_pieces[i]);
        //     score -= WEIGHTS[i] / (20 + 5 * (abs(player_king_y - opponent_piece_y) + abs(player_king_x - opponent_piece_x)));
        // }
    };

    add_piece_score();
    add_promo_score();
    add_king_distance_score();
    subtract_check_score();

    score.update_total();

    return score;
}

bool is_equal(Board* b1, Board* b2) {
    bool is_king_equal = (b1->data.b_king == b2->data.b_king) && (b1->data.w_king == b2->data.w_king);
    bool is_rook_ws_equal = (b1->data.b_rook_ws == b2->data.b_rook_ws) && (b1->data.w_rook_ws == b2->data.w_rook_ws);
    bool is_rook_bs_equal = (b1->data.b_rook_bs == b2->data.b_rook_bs) && (b1->data.w_rook_bs == b2->data.w_rook_bs);
    bool is_pawn_ws_equal = (b1->data.b_pawn_ws == b2->data.b_pawn_ws) && (b1->data.w_pawn_ws == b2->data.w_pawn_ws);
    bool is_pawn_bs_equal = (b1->data.b_pawn_bs == b2->data.b_pawn_bs) && (b1->data.w_pawn_bs == b2->data.w_pawn_bs);
    bool is_bishop_equal = (b1->data.b_bishop == b2->data.b_bishop) && (b1->data.w_bishop == b2->data.w_bishop);
    bool is_equal = is_king_equal && is_rook_ws_equal && is_rook_bs_equal && is_pawn_ws_equal && is_pawn_bs_equal && is_bishop_equal;
    return is_equal;
}


Evaluation minimax(Board& board, int depth, bool maximizing_player, vector<Board*> &visited, int alpha, int beta) {
    if (depth == 0) {
        return eval(board);
    }

    Evaluation best_eval;
    best_eval.total = (maximizing_player ? INT_MIN : INT_MAX);
    auto player_moveset = board.get_legal_moves();
    if (player_moveset.empty() && !board.in_check()) {
        best_eval.total = (maximizing_player ? 1 : -1) * STALEMATE_WEIGHT;
        return best_eval;
    }
    for (auto move : player_moveset) {
        Board* new_board = board.copy();
        new_board->do_move(move);
        bool is_visited_board = false;
        for (Board* b : visited) {
            if (is_equal(b, new_board)) {
                is_visited_board = true;
                break;
            }
        }
        if (is_visited_board) {
            free(new_board);
            continue;
        }
        visited.push_back(new_board);
        nodes_visited++;
        Evaluation eval = minimax(*new_board, depth - 1, !maximizing_player, visited, alpha, beta);
        free(new_board);
        visited.pop_back();
        if (maximizing_player) {
            best_eval = max(best_eval, eval, Evaluation::compare);
            alpha = max(alpha, best_eval.total);
        } else {
            best_eval = min(best_eval, eval, Evaluation::compare);
            beta = min(beta, best_eval.total);
        }
        if (alpha >= beta) {
            break;
        }
    }
    return best_eval;
}

void Engine::find_best_move(const Board& b) {
    auto start_time = chrono::high_resolution_clock::now();
    previous_board_occurences[all_boards_to_str(b)]++;
    if (curr_player == -1) {
        curr_player = b.data.player_to_play;
        init_quadrant_map();
    }
    Evaluation best_eval;
    best_eval.total = INT_MIN;
    U16 best_move = 0;
    auto player_moveset = b.get_legal_moves();
    this->best_move = 0;
    // std::cout << all_boards_to_str(b) << std::endl;
    // for (auto m : moveset) {
    //     std::cout << move_to_str(m) << " ";
    // }
    // std::cout << std::endl;
    vector<Board*> visited;
    nodes_visited = 0;
    int alpha = INT_MIN;
    int beta = INT_MAX;
    for (auto move : player_moveset) {
        Board* new_board = b.copy();
        new_board->do_move(move);
        visited.push_back(new_board);
        nodes_visited++;
        Evaluation eval = minimax(*new_board, SEARCH_DEPTH - 1, false, visited, alpha, beta);
        visited.pop_back();
        free(new_board);
        if (eval > best_eval || best_eval.total == INT_MIN) {
            best_eval = eval;
            best_move = move;
            alpha = eval.total;
        }
    }
    this->best_move = best_move;
    auto end_time = chrono::high_resolution_clock::now();
    Board* new_board = b.copy();
    new_board->do_move(best_move);
    previous_board_occurences[all_boards_to_str(*new_board)]++;
    free(new_board);
    best_eval.print();
    cout << get_distance(pos(2, 1), pos(4, 5));
    cout << "time taken to find best move: " << chrono::duration_cast<chrono::duration<double>>(end_time - start_time).count() << " seconds" << endl;
    cout << "nodes visited " << nodes_visited << endl;
}
